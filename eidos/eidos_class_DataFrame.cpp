//
//  eidos_class_DataFrame.cpp
//  Eidos
//
//  Created by Ben Haller on 10/10/21.
//  Copyright (c) 2021-2023 Philipp Messer.  All rights reserved.
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

#include "eidos_class_DataFrame.h"
#include "eidos_globals.h"
#include "eidos_functions.h"
#include "eidos_interpreter.h"

#include <fstream>
#include <regex>
#include <limits>
#include <string>
#include <algorithm>
#include <vector>


//
// EidosDataFrame
//
#pragma mark -
#pragma mark EidosDataFrame
#pragma mark -

void EidosDataFrame::Raise_UsesStringKeys(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosDataFrame::Raise_UsesStringKeys): cannot use an integer key with the target DataFrame object; DataFrame always uses string keys." << EidosTerminate(nullptr);
}

int EidosDataFrame::RowCount(void) const
{
	AssertKeysAreStrings();
	
	// Get our row count; we don't cache this so we don't have to worry about validating the cache
	const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
	
	if (!symbols || (symbols->size() == 0))
		return 0;
	
	auto iter = symbols->begin();
	
	return iter->second->Count();
}

EidosDataFrame *EidosDataFrame::SubsetColumns(EidosValue *index_value)
{
	AssertKeysAreStrings();
	
	EidosDataFrame *dataframe = new EidosDataFrame();
	
	try {
		EidosValueType index_type = index_value->Type();
		int index_count = index_value->Count();
		const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
		
		if (!symbols)
		{
			// With no columns, we either throw an error (if columns were selected) or return an empty DataFrame
			if (index_count > 0)
				EIDOS_TERMINATION << "ERROR (EidosDataFrame::SubsetColumns): cannot select columns from an empty DataFrame." << EidosTerminate(nullptr);
			
			return dataframe;
		}
		
		const std::vector<std::string> keys = SortedKeys_StringKeys();	// if symbols_ptr is not nullptr, this is also not nullptr
		
		if (index_type == EidosValueType::kValueInt)
		{
			int64_t key_count = (int64_t)keys.size();
			
			for (int i = 0; i < index_count; ++i)
			{
				int64_t index = index_value->IntAtIndex(i, nullptr);
				
				if ((index < 0) || (index >= key_count))
					EIDOS_TERMINATION << "ERROR (EidosDataFrame::SubsetColumns): column index out of range (" << index << " not in [0, " << (key_count - 1) << "])." << EidosTerminate(nullptr);
				
				const std::string &key = keys[index];
				auto value_iter = symbols->find(key);
				
				if (value_iter == symbols->end())
					EIDOS_TERMINATION << "ERROR (EidosDataFrame::SubsetColumns): (internal error) no value for defined key." << EidosTerminate(nullptr);
				
				dataframe->SetKeyValue_StringKeys(key, value_iter->second);
			}
		}
		else if (index_type == EidosValueType::kValueString)
		{
			for (int i = 0; i < index_count; ++i)
			{
				const std::string &key = ((EidosValue_String *)index_value)->StringRefAtIndex(i, nullptr);
				
				auto value_iter = symbols->find(key);
				
				if (value_iter == symbols->end())
					EIDOS_TERMINATION << "ERROR (EidosDataFrame::SubsetColumns): key " << key << " is not defined in the target DataFrame." << EidosTerminate(nullptr);
				
				dataframe->SetKeyValue_StringKeys(key, value_iter->second);
			}
		}
		else // (index_type == EidosValueType::kValueLogical)
		{
			int64_t symbols_count = (int64_t)symbols->size();
			
			if (index_count != symbols_count)
				EIDOS_TERMINATION << "ERROR (EidosDataFrame::SubsetColumns): logical index vector length does not match the number of columns in the DataFrame." << EidosTerminate(nullptr);
			
			for (int i = 0; i < index_count; ++i)
			{
				bool selected = index_value->LogicalAtIndex(i, nullptr);
				
				if (selected)
				{
					const std::string &key = keys[i];
					auto value_iter = symbols->find(key);
					
					if (value_iter == symbols->end())
						EIDOS_TERMINATION << "ERROR (EidosDataFrame::SubsetColumns): (internal error) no value for defined key." << EidosTerminate(nullptr);
					
					dataframe->SetKeyValue_StringKeys(key, value_iter->second);
				}
			}
		}
		
		return dataframe;
	} catch (...) {
		dataframe->Release();
		throw;
	}
}

EidosDataFrame *EidosDataFrame::SubsetRows(EidosValue *index_value, bool drop)
{
	AssertKeysAreStrings();
	
	EidosDataFrame *dataframe = new EidosDataFrame();
	
	try {
		// With no columns, the indices don't matter, and the result is a new empty dictionary
		const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
		
		if (!symbols || (symbols->size() == 0))
			return dataframe;
		
		// Otherwise, we subset to get the result value for each key we contain
		const std::vector<std::string> keys = SortedKeys_StringKeys();
		
		for (const std::string &key : keys)
		{
			auto kv_pair = symbols->find(key);
			
			if (kv_pair == symbols->end())
				EIDOS_TERMINATION << "ERROR (EidosDataFrame::SubsetRows): (internal error) key not found in symbols." << EidosTerminate(nullptr);
			
			EidosValue_SP subset = SubsetEidosValue(kv_pair->second.get(), index_value, nullptr, /* p_raise_range_errors */ true);
			
			if (!drop || subset->Count())
				dataframe->SetKeyValue_StringKeys(kv_pair->first, subset);
		}
		
		return dataframe;
	} catch (...) {
		dataframe->Release();
		throw;
	}
}

std::vector<std::string> EidosDataFrame::SortedKeys_StringKeys(void) const
{
	AssertKeysAreStrings();
	
	// Provide our keys in their user-defined order (without sorting as Dictionary does)
	return sorted_keys_;
}

std::vector<int64_t> EidosDataFrame::SortedKeys_IntegerKeys(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosDataFrame::SortedKeys_IntegerKeys): (internal error) DataFrame does not support integer keys." << EidosTerminate(nullptr);
}

void EidosDataFrame::KeyAddedToDictionary_StringKeys(const std::string &p_key)
{
	if (!state_ptr_)
		EIDOS_TERMINATION << "ERROR (EidosDataFrame::KeyAddedToDictionary_StringKeys): (internal error) no state_ptr_." << EidosTerminate(nullptr);
	
	AssertKeysAreStrings();
	
	// call super
	super::KeyAddedToDictionary_StringKeys(p_key);
	
	// Maintain our user-defined key ordering
	auto iter = std::find(sorted_keys_.begin(), sorted_keys_.end(), p_key);
	
	if (iter == sorted_keys_.end())
		sorted_keys_.emplace_back(p_key);
}

void EidosDataFrame::KeyAddedToDictionary_IntegerKeys(__attribute__((unused)) int64_t p_key)
{
	EIDOS_TERMINATION << "ERROR (EidosDataFrame::KeyAddedToDictionary_IntegerKeys): (internal error) DataFrame does not support integer keys." << EidosTerminate(nullptr);
}

void EidosDataFrame::KeyRemovedFromDictionary_StringKeys(const std::string &p_key)
{
	// call super
	super::KeyRemovedFromDictionary_StringKeys(p_key);
	
	// Maintain our user-defined key ordering
	auto iter = std::find(sorted_keys_.begin(), sorted_keys_.end(), p_key);
	
	if (iter != sorted_keys_.end())
		sorted_keys_.erase(iter);
}

void EidosDataFrame::KeyRemovedFromDictionary_IntegerKeys(__attribute__((unused)) int64_t p_key)
{
	EIDOS_TERMINATION << "ERROR (EidosDataFrame::KeyRemovedFromDictionary_IntegerKeys): (internal error) DataFrame does not support integer keys." << EidosTerminate(nullptr);
}

void EidosDataFrame::AllKeysRemoved(void)
{
	// call super
	super::AllKeysRemoved();
	
	// Maintain our user-defined key ordering
	sorted_keys_.clear();
}

void EidosDataFrame::ContentsChanged(const std::string &p_operation_name)
{
	AssertKeysAreStrings();
	
	// call super
	super::ContentsChanged(p_operation_name);
	
	if (!state_ptr_)
		return;
	
	const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
	
	// Check that sorted_keys_ matches DictionarySymbols_StringKeys()
	if (symbols->size() != sorted_keys_.size())
		EIDOS_TERMINATION << "ERROR (EidosDataFrame::ContentsChanged): (internal error) DataFrame found key count mismatch after " << p_operation_name << "." << EidosTerminate(nullptr);
	
	// Go through all of our columns and check that they are the same size
	// Also check that all are simple vectors, not matrices or arrays
	int row_count = -1;
	
	if (!symbols)
		return;
	
	for (auto const &kv_pair : *symbols)
	{
		const EidosValue *value = kv_pair.second.get();
		int value_count = value->Count();
		
		if (row_count == -1)
			row_count = value_count;
		else if (row_count != value_count)
			EIDOS_TERMINATION << "ERROR (EidosDataFrame::ContentsChanged): DataFrame found inconsistent column sizes after " << p_operation_name << "; all columns must be the same length." << EidosTerminate(nullptr);
		
		if (value->DimensionCount() != 1)
			EIDOS_TERMINATION << "ERROR (EidosDataFrame::ContentsChanged): DataFrame found a matrix or array value after " << p_operation_name << "; only vector values are allowed in DataFrame." << EidosTerminate(nullptr);
	}
}

const EidosClass *EidosDataFrame::Class(void) const
{
	return gEidosDataFrame_Class;
}

void EidosDataFrame::Print(std::ostream &p_ostream) const
{
	// We want to print as a data table (rows and columns), not as a dictionary (keys with associated values).  The layout is the tricky thing.
	// We have to pre-plan our output: go through all of our elements, generate their output string, and calculate the width of each column.
	// We put all of those strings into a temporary data structure that we build here, organized by column.  We thus keep all the output in
	// memory; that would be an issue for a *very* large dataframe, but that seems unlikely.
	const std::vector<std::string> keys = SortedKeys_StringKeys();
	const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
	std::ostringstream ss;
	
	if (keys.size())
	{
		// First, assemble our planned output
		std::vector<std::vector<std::string>> output;
		std::vector<int> column_widths;
		int row_count = RowCount(), col_count = ColumnCount();
		
		for (const std::string &key : keys)
		{
			std::vector<std::string> col_output;
			
			// Output the column header (i.e., the key), using quotes only if needed
			EidosStringQuoting key_quoting = EidosStringQuoting::kNoQuotes;
			
			if (key.find_first_of("\"\'\\\r\n\t =;") != std::string::npos)
				key_quoting = EidosStringQuoting::kDoubleQuotes;		// if we use quotes, always use double quotes, for ease of parsing
			
			ss << Eidos_string_escaped(key, key_quoting);
			col_output.emplace_back(ss.str());
			ss.clear();
			ss.str(gEidosStr_empty_string);
			
			// Output all of the values in the column
			auto key_iter = symbols->find(key);
			
			if (key_iter != symbols->end())
			{
				EidosValue *value = key_iter->second.get();
				int value_count = value->Count();
				
				for (int value_index = 0; value_index < value_count; ++value_index)
				{
					value->PrintValueAtIndex(value_index, ss);
					col_output.emplace_back(ss.str());
					ss.clear();
					ss.str(gEidosStr_empty_string);
				}
			}
			
			// Calculate the column width
			int max_width = 0;
			
			for (auto &col_string : col_output)
				max_width = std::max(max_width, (int)col_string.length());
			
			// Save the results
			output.emplace_back(col_output);
			column_widths.emplace_back(max_width);
		}
		
		// Figure out the width for the row numbers
		int max_row_number = row_count - 1;
		int row_num_width;
		
		ss << max_row_number;
		row_num_width = (int)ss.str().length();
		ss.clear();
		ss.str(gEidosStr_empty_string);
		
		// Then, generate the output
		for (int row = 0; row <= row_count; ++row)		// <= to include the header row, which is row 0 here
		{
			for (int col = -1; col < col_count; ++col)	// -1 to include the row numbers, which are col -1 here since they aren't in `output`
			{
				if (col == -1)
				{
					// Emit the row numbers
					if (row == 0)
						p_ostream << std::string(row_num_width, ' ');
					else
					{
						ss << (row - 1);
						
						std::string row_str = ss.str();
						
						p_ostream << std::string(row_num_width - row_str.length(), ' ') << row_str;
						
						ss.clear();
						ss.str(gEidosStr_empty_string);
					}
				}
				else
				{
					// Emit our pre-planned strings
					const std::string &out_str = output[col][row];
					
					p_ostream << std::string(column_widths[col] - out_str.length() + 1, ' ') << out_str;
				}
			}
			
			// newlines for everything but the last line
			if (row < row_count)
				p_ostream << std::endl;
		}
		
		if (row_count == 0)
		{
			p_ostream << std::endl << "<0 rows>";
		}
	}
	else
	{
		p_ostream << "DataFrame with 0 columns and 0 rows" << std::endl;
	}
}

EidosValue_SP EidosDataFrame::GetProperty(EidosGlobalStringID p_property_id)
{
#if DEBUG
	// Check for correctness before dispatching out; perhaps excessively cautious, but checks are good
	ContentsChanged("EidosDataFrame::GetProperty");
#endif
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gEidosID_colNames:
			return AllKeys();
		case gEidosID_dim:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{RowCount(), ColumnCount()});
		case gEidosID_ncol:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(ColumnCount()));
		case gEidosID_nrow:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(RowCount()));
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue_SP EidosDataFrame::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#if DEBUG
	// Check for correctness before dispatching out; perhaps excessively cautious, but checks are good
	ContentsChanged("EidosDataFrame::ExecuteInstanceMethod");
#endif
	
	switch (p_method_id)
	{
		case gEidosID_asMatrix:					return ExecuteMethod_asMatrix(p_method_id, p_arguments, p_interpreter);
		case gEidosID_cbind:					return ExecuteMethod_cbind(p_method_id, p_arguments, p_interpreter);
		case gEidosID_rbind:					return ExecuteMethod_rbind(p_method_id, p_arguments, p_interpreter);
		case gEidosID_subset:					return ExecuteMethod_subset(p_method_id, p_arguments, p_interpreter);
		case gEidosID_subsetColumns:			return ExecuteMethod_subsetColumns(p_method_id, p_arguments, p_interpreter);
		case gEidosID_subsetRows:				return ExecuteMethod_subsetRows(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (void)asMatrix(void)
//
EidosValue_SP EidosDataFrame::ExecuteMethod_asMatrix(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	AssertKeysAreStrings();
	
	// First determine what type the matrix would be, and check that all columns match that type
	int64_t nrow = RowCount();
	const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
	int64_t ncol = symbols->size();
	EidosValue_SP type_template;
	const EidosClass *class_template = nullptr;
	
	if (ncol == 0)
	{
		type_template = gStaticEidosValue_Logical_ZeroVec;		// with no columns, we have no way to know the type, so we go with "logical", following R
	}
	else
	{
		for (auto const &symbols_iter : *symbols)
		{
			if (!type_template)
			{
				type_template = symbols_iter.second;
				if (type_template->Type() == EidosValueType::kValueObject)
					class_template = ((EidosValue_Object *)(type_template.get()))->Class();
			}
			else if (type_template->Type() != symbols_iter.second->Type())
			{
				EIDOS_TERMINATION << "ERROR (EidosDataFrame::ExecuteMethod_asMatrix): asMatrix() requires that every column of the target DataFrame is the same type (" << type_template->Type() << " != " << symbols_iter.second->Type() << ")." << EidosTerminate(nullptr);
			}
			else if (class_template)
			{
				const EidosClass *class_column = ((EidosValue_Object *)(symbols_iter.second.get()))->Class();
				
				if (class_template != class_column)
					EIDOS_TERMINATION << "ERROR (EidosDataFrame::ExecuteMethod_asMatrix): asMatrix() requires that every object element in the target DataFrame is the same class (" << class_template->ClassName() << " != " << class_column->ClassName() << ")." << EidosTerminate(nullptr);
			}
		}
	}
	
	// Create the matrix; for now we use a slow implementation that is type-agnostic and does not resize to fit first, probably this is unlikely to be a bottleneck
	//int64_t data_count = nrow * ncol;
	EidosValue_SP result_SP = type_template->NewMatchingType();
	EidosValue *result = result_SP.get();
	
	//result_SP->resize_no_initialize(data_count);
	
	// Fill in all the values, in sorted column order
	const std::vector<std::string> keys = SortedKeys_StringKeys();
	
	for (const std::string &key : keys)
	{
		auto key_iter = symbols->find(key);
		
		if (key_iter == symbols->end())
			EIDOS_TERMINATION << "ERROR (EidosDataFrame::ExecuteMethod_asMatrix): (internal error) key not found." << EidosTerminate(nullptr);
		
		EidosValue *column_value = key_iter->second.get();
		
		for (int64_t i = 0; i < nrow; ++i)
			result->PushValueFromIndexOfEidosValue((int)i, *column_value, nullptr);
	}
	
	const int64_t dim_buf[2] = {nrow, ncol};
	
	result_SP->SetDimensions(2, dim_buf);
	
	return result_SP;
}

//	*********************	- (void)cbind(object source, ...)
//
EidosValue_SP EidosDataFrame::ExecuteMethod_cbind(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	AssertKeysAreStrings();
	
	// This method is similar to addKeysAndValuesFrom(), with a couple of differences.  One, a collision in column names
	// is an error, rather than the existing column being replaced.  Two, the lengths of all columns must be the same
	// (basic DataFrame requirement).  Three, this method handles multiple adds; source does not have to be a singleton,
	// and the ellipsis can contain further Dictionary/DataFrame arguments, which also don't have to be singleton.  We
	// can use AddKeysAndValuesFrom() to do the work for us under the hood.
	for (const EidosValue_SP &arg : p_arguments)
	{
		int arg_count = arg->Count();
		
		for (int arg_index = 0; arg_index < arg_count; ++arg_index)
		{
			EidosObject *source_obj = arg->ObjectElementAtIndex(arg_index, nullptr);
			EidosDictionaryUnretained *source = dynamic_cast<EidosDictionaryUnretained *>(source_obj);
			
			if (!source)
				EIDOS_TERMINATION << "ERROR (EidosDataFrame::ExecuteMethod_cbind): cbind() can only take values from a Dictionary or a subclass of Dictionary." << EidosTerminate(nullptr);
			
			AddKeysAndValuesFrom(source, /* p_allow_replace */ false);
		}
	}
	
	ContentsChanged("cbind()");
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)rbind(object source, ...)
//
EidosValue_SP EidosDataFrame::ExecuteMethod_rbind(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	AssertKeysAreStrings();
	
	// This method is similar to appendKeysAndValuesFrom(), with a couple of differences.  One, the column names of
	// the dictionary being appended must match in value and order.  Two, the lengths of all columns must be the same
	// (basic DataFrame requirement).  Three, this method handles multiple adds; source does not have to be a singleton,
	// and the ellipsis can contain further Dictionary/DataFrame arguments, which also don't have to be singleton.  We
	// can use AppendKeysAndValuesFrom() to do the work for us under the hood.
	for (const EidosValue_SP &arg : p_arguments)
	{
		int arg_count = arg->Count();
		
		for (int arg_index = 0; arg_index < arg_count; ++arg_index)
		{
			EidosObject *source_obj = arg->ObjectElementAtIndex(arg_index, nullptr);
			EidosDictionaryUnretained *source = dynamic_cast<EidosDictionaryUnretained *>(source_obj);
			
			if (!source)
				EIDOS_TERMINATION << "ERROR (EidosDataFrame::ExecuteMethod_rbind): rbind() can only take values from a Dictionary or a subclass of Dictionary." << EidosTerminate(nullptr);
			
			AppendKeysAndValuesFrom(source, /* p_require_column_match */ true);
		}
	}
	
	ContentsChanged("rbind()");
	
	return gStaticEidosValueVOID;
}

//	*********************	- (*)subset([Nli rows = NULL], [Nlis cols = NULL])
//
EidosValue_SP EidosDataFrame::ExecuteMethod_subset(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	AssertKeysAreStrings();
	
	EidosValue *rows_value = p_arguments[0].get();
	EidosValue *cols_value = p_arguments[1].get();
	EidosValue_SP result_SP(nullptr);
	
	// First subset the rows
	EidosDataFrame *rows_subset;
	
	if (rows_value->Type() == EidosValueType::kValueNULL)
	{
		rows_subset = this;
		rows_subset->Retain();
	}
	else
	{
		rows_subset = SubsetRows(rows_value);
		rows_subset->ContentsChanged("subset()");
	}
	
	try {
		// Then subset the columns
		EidosDataFrame *cols_subset;
		
		if (cols_value->Type() == EidosValueType::kValueNULL)
		{
			cols_subset = rows_subset;
			cols_subset->Retain();
		}
		else
		{
			cols_subset = rows_subset->SubsetColumns(cols_value);
			cols_subset->ContentsChanged("subset()");
		}
		
		// Then return the resulting DataFrame, or if it contains exactly one column, return the vector of values from that column instead
		if (cols_subset->ColumnCount() == 1)
		{
			const EidosDictionaryHashTable_StringKeys *symbols = cols_subset->DictionarySymbols_StringKeys();
			auto col_iter = symbols->begin();
			
			result_SP = col_iter->second;
		}
		else
		{
			// Note that this retains cols_subset, before the call to Release below
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(cols_subset, gEidosDataFrame_Class));
		}
		
		rows_subset->Release();
		cols_subset->Release();
		return result_SP;
	}
	catch (...) {
		rows_subset->Release();
		throw;
	}
}

//	*********************	- (object<DataFrame>$)subsetColumns(lis index)
//
EidosValue_SP EidosDataFrame::ExecuteMethod_subsetColumns(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	AssertKeysAreStrings();
	
	EidosValue *index_value = p_arguments[0].get();
	EidosDataFrame *objectElement = SubsetColumns(index_value);
	objectElement->ContentsChanged("subsetColumns()");
	
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDataFrame_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}

//	*********************	- (object<DataFrame>$)subsetRows(li index, [logical$ drop = F])
//
EidosValue_SP EidosDataFrame::ExecuteMethod_subsetRows(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	AssertKeysAreStrings();
	
	EidosValue *index_value = p_arguments[0].get();
	EidosValue *drop_value = p_arguments[1].get();
	EidosDataFrame *objectElement = SubsetRows(index_value, drop_value->LogicalAtIndex(0, nullptr));
	objectElement->ContentsChanged("subsetRows()");
	
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDataFrame_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}


//
//	Object instantiation
//
#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	(object<DataFrame>$)DataFrame(...)
static EidosValue_SP Eidos_Instantiate_EidosDataFrame(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosDataFrame *objectElement = new EidosDataFrame();
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDataFrame_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	// now use a constructor that we share with Dictionary
	objectElement->ConstructFromEidos(p_arguments, p_interpreter, "Eidos_Instantiate_EidosDataFrame" , "DataFrame");
	objectElement->ContentsChanged("DataFrame()");
	
	return result_SP;
}

//	(object<DataFrame>$)readCSV(string$ filePath, [ls colNames = T], [Ns$ colTypes = NULL], [string$ sep = ","], [string$ quote = "\""], [string$ dec = "."], [string$ comment = ""])
static EidosValue_SP Eidos_ExecuteFunction_readCSV(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *colNames_value = p_arguments[1].get();
	EidosValue *colTypes_value = p_arguments[2].get();
	EidosValue *sep_value = p_arguments[3].get();
	EidosValue *quote_value = p_arguments[4].get();
	EidosValue *dec_value = p_arguments[5].get();
	EidosValue *comment_value = p_arguments[6].get();
	
	// Start by opening the CSV data file; a little weird that we just warn and retunr NULL on a file I/O error, but this follows readFile()
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	std::ifstream file_stream(file_path.c_str());
	
	if (!file_stream.is_open())
	{
		if (!gEidosSuppressWarnings)
			p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_readCSV): function readCSV() could not read file at path " << file_path << "." << std::endl;
		return gStaticEidosValueNULL;
	}

	// Figure out our various separators/delimiters
	std::string sep_string = sep_value->StringAtIndex(0, nullptr);
	std::string quote_string = quote_value->StringAtIndex(0, nullptr);
	std::string dec_string = dec_value->StringAtIndex(0, nullptr);
	std::string comment_string = comment_value->StringAtIndex(0, nullptr);
	
	if (sep_string.length() > 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires that sep be a string of exactly one character, or the empty string \"\"." << EidosTerminate(nullptr);
	if (quote_string.length() != 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires that quote be a string of exactly one character." << EidosTerminate(nullptr);
	if (dec_string.length() != 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires that dec be a string of exactly one character." << EidosTerminate(nullptr);
	if (comment_string.length() > 1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires that comment be a string of exactly one character, or the empty string." << EidosTerminate(nullptr);
	
	char sep = (sep_string.length() ? sep_string[0] : 0);				// 0 indicates "whitespace separator", a special case
	char quote = quote_string[0];
	char dec = dec_string[0];
	char comment = (comment_string.length() ? comment_string[0] : 0);	// 0 indicates "no comments"
	
	if ((sep && ((sep == quote) || (sep == dec) || (sep == comment))) ||
		((quote == dec) || (quote == comment) || (dec == comment)))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires sep, quote, dec, and comment to be different from each other." << EidosTerminate(nullptr);
	if (!std::isprint(dec) || std::isalnum(dec) || (dec == '+') || (dec == '-'))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires that dec be a printable, non-alphanumeric character that is not '+' or '-' (typically '.' or ',')." << EidosTerminate(nullptr);
	
	// Read lines and split each line up into components; this is non-trivial since it involves parsing out quoted strings and unquoting them
	// Check that each line has the same number of components as we go along, to avoid having to make an extra pass
	std::string line;
	std::vector<std::vector<std::string>> rows;
	int ncols = -1, line_number = 0;
	
	while (getline(file_stream, line))
	{
		// split line into strings based on sep, quote, and comment
		std::vector<std::string> row;
		const char *line_ptr = line.data();
		char ch = *line_ptr;
		
		line_number++;		// after this increment, this has the line number (1-based) we are current parsing
		
		// a line is allowed to be completely empty, or to be a comment line (starting at its first character)
		if ((ch == 0) || (comment && (ch == comment)))
			continue;
		
		// if the separator is "whitespace" the line can begin with whitespace, which we eat here
		if (!sep)
			while ((ch == ' ') || (ch == '\t'))
				ch = *(++line_ptr);
		
		do
		{
			// ch should always be equal to *line_ptr here already, no need to fetch it again
			bool line_ended_without_separator = false;
			
			// at the top of the loop, we expect a new element; a comment or a null means we have an empty string and then end
			// this might look like: foo,bar,baz,#comment: the last element is an empty string
			// if the separator is "whitespace" then an empty string is not implied here; we just end the line
			if ((ch == 0) || (comment && (ch == comment)))
			{
				// empty element (if the separator is not whitespace), and then end the line
				if (sep)
					row.emplace_back();
				break;
			}
			
			// similarly, a separator character here means we have an empty string and then expect another element
			// we make the empty element, eat the separator, and loop back for the next element
			// note this does not occur for a "whitespace" separator; any whitespace would already be eaten at this point,
			// because two consecutive "whitespace" separators cannot occur, whereas ",," can occur implying an empty string
			if (ch == sep)
			{
				row.emplace_back();
				ch = *(++line_ptr);
				continue;
			}
			
			// we are at the start of a new element, which must be either quoted or unquoted
			// note that leading whitespace is part of the element, and indicates an unquoted element
			if (ch == quote)
			{
				// quoted string: read until the end quote, unquoting doubled quotes
				std::string element_string;
				
				// eat the quote and get the next character
				ch = *(++line_ptr);
				
				do
				{
					// ch should always be the next character to look at in the element, at this point; no need to fetch it again
					
					// at the top of the loop, we are inside the quoting and the next character either terminates the quoting or continues inside it
					if (ch == 0)
					{
						// we reached the end of the line, but we're still inside the quoted element; incorporate the implied newline and keep going
						if (!getline(file_stream, line))
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): function readCSV() encountered an unexpected end-of-file inside a quoted element, at line " << line_number << "." << EidosTerminate(nullptr);
						
						element_string.append(1, '\n');
						line_number++;
						line_ptr = line.data();
						ch = *line_ptr;
					}
					else if (ch == quote)
					{
						// we hit a quote character; if the *next* character is also a quote, then we have a double quote,
						// which is an escape indicating a single quote, otherwise we have terminated the element
						ch = *(++line_ptr);
						
						if (ch == quote)
						{
							// doubled quote; append one quote and continue
							element_string.append(1, quote);
							ch = *(++line_ptr);
						}
						else
						{
							// not a doubled quote; the element is terminated and ch is already the character after the end quote
							// at this point, we expect only a separator, a comment, or a line end; the element is done
							if (sep && (ch == sep))
							{
								ch = *(++line_ptr);
								break;
							}
							else if (!sep && ((ch == ' ') || (ch == '\t')))
							{
								// eat a "whitespace" separator, similar to above
								while ((ch == ' ') || (ch == '\t'))
									ch = *(++line_ptr);
								break;
							}
							else if ((ch == 0) || (comment && (ch == comment)))
							{
								line_ended_without_separator = true;
								break;
							}
							else
							{
								EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): function readCSV() encountered an unexpected character '" << ch << "' after the end of a quoted element." << EidosTerminate(nullptr);
							}
						}
					}
					else
					{
						// this character is part of the element; the above cases are the only exceptions
						element_string.append(1, ch);
						ch = *(++line_ptr);
					}
				}
				while (true);
				
				// add the completed element to the row
				row.emplace_back(element_string);
			}
			else
			{
				// unquoted string: read until a separator, comment, or null
				std::string element_string;
				
				do
				{
					// at the top of the loop, ch has a valid character to be added to the element; do so
					element_string.append(1, ch);
					ch = *(++line_ptr);
					
					// now decide what to do about the next character
					// NOLINTBEGIN(*-branch-clone) : intentional branch clones
					if (ch == 0)
					{
						// we reached the end of the line, which terminates the element
						line_ended_without_separator = true;
						break;
					}
					else if (sep && (ch == sep))
					{
						// we hit a separator, which terminates the element but expects another
						// eat the separator so we're at the start of the next element
						ch = *(++line_ptr);
						break;
					}
					else if (!sep && ((ch == ' ') || (ch == '\t')))
					{
						// eat a "whitespace" separator, similar to above
						while ((ch == ' ') || (ch == '\t'))
							ch = *(++line_ptr);
						break;
					}
					else if (comment && (ch == comment))
					{
						// we hit a comment character, which terminates the element
						line_ended_without_separator = true;
						break;
					}
					// NOLINTEND(*-branch-clone)
					
					// the character is part of the element; let the top of the loop handle it
				}
				while (true);
				
				// add the completed element to the row
				row.emplace_back(element_string);
			}
			
			// if we ended the line above without seeing a separator, we do not expect another element; the row is done
			// this flag is effectively a way of breaking out of this outer loop from inside a nested loop
			if (line_ended_without_separator)
				break;
		}
		while (true);
		
		// check the column count, and if it passes, append this row to our buffer and move on
		if (ncols == -1)
			ncols = (int)row.size();
		else if (ncols != (int)row.size())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): function readCSV() encountered an inconsistent column count in CSV file (" << row.size() << " observed, " << ncols << " previously), at line " << line_number << "." << EidosTerminate(nullptr);
		
		rows.emplace_back(row);
	}
	
	if (file_stream.bad())
	{
		if (!gEidosSuppressWarnings)
			p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_readCSV): function readCSV() encountered stream errors while reading file at path " << file_path << "." << std::endl;
		return gStaticEidosValueNULL;
	}
	
	// Decide on the name for each column, using colNames and/or defaults
	// If a header line is expected, this removes the first input line to act as the header
	std::vector<std::string> columnNames;
	
	if ((colNames_value->Type() == EidosValueType::kValueLogical) && (colNames_value->Count() == 1) && (colNames_value->LogicalAtIndex(0, nullptr) == true))
	{
		// colNames == T means "a header row is present, use it"
		if (rows.size() == 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() found no header row, but colNames==T indicating that one is expected." << EidosTerminate(nullptr);
		
		columnNames = rows[0];
		rows.erase(rows.begin());
	}
	else if ((colNames_value->Type() == EidosValueType::kValueLogical) && (colNames_value->Count() == 1) && (colNames_value->LogicalAtIndex(0, nullptr) == false))
	{
		// colNames == F means "autogenerate column names of the form X1, X2, ..."
		for (int col_index = 0; col_index < ncols; ++col_index)
			columnNames.emplace_back(std::string("X") + std::to_string(col_index + 1));
	}
	else if (colNames_value->Type() == EidosValueType::kValueString)
	{
		// colNames as a string vector supplies column names, but might run out and then we autogenerate
		int colNames_count = colNames_value->Count();
		
		for (int col_index = 0; col_index < ncols; ++col_index)
		{
			if (col_index < colNames_count)
			{
				// The name is provided by colNames
				std::string colname = colNames_value->StringAtIndex(col_index, nullptr);
				auto check_iter = std::find(columnNames.begin(), columnNames.end(), colname);
				
				if (check_iter != columnNames.end())
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires unique column names, but '" << colname << "' is not unique." << EidosTerminate(nullptr);
				
				columnNames.emplace_back(colname);
			}
			else
			{
				// The name must be autogenerated; try X1, X2, ... starting at the current column index until we find an unused name
				int candidate_index = col_index + 1;
				std::string candidate_name;
				
				do
				{
					candidate_name = std::string("X") + std::to_string(candidate_index);
					
					auto check_iter = std::find(columnNames.begin(), columnNames.end(), candidate_name);
					
					if (check_iter == columnNames.end())
						break;
					
					candidate_index++;
				}
				while (true);
				
				columnNames.emplace_back(candidate_name);
			}
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() requires colNames to be T, F, or a string vector of column names." << EidosTerminate(nullptr);
	}
	
	// Decide on a type for each column, using colTypes and/or guesses; we use kValueVOID to mean "skip this column", and kValueNULL to mean "guess this column"
	std::vector<EidosValueType> coltypes;
	bool has_null_coltype = false;
	
	if (colTypes_value->Type() == EidosValueType::kValueString)
	{
		std::string colTypes_string = colTypes_value->StringAtIndex(0, nullptr);
		
		for (char ch : colTypes_string)
		{
			switch (ch) {
				case 'l': coltypes.emplace_back(EidosValueType::kValueLogical); break;
				case 'i': coltypes.emplace_back(EidosValueType::kValueInt); break;
				case 'f': coltypes.emplace_back(EidosValueType::kValueFloat); break;
				case 's': coltypes.emplace_back(EidosValueType::kValueString); break;
				case '?': coltypes.emplace_back(EidosValueType::kValueNULL); has_null_coltype = true; break;
				case '_':
				case '-': coltypes.emplace_back(EidosValueType::kValueVOID); break;
				default:
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): readCSV() did not recognize column type '" << ch << "' in colTypes." << EidosTerminate(nullptr);
			}
		}
	}
	
	while ((int)coltypes.size() < ncols)
	{
		coltypes.emplace_back(EidosValueType::kValueNULL);		// guess by default
		has_null_coltype = true;
	}
	
	// Resolve the type for columns that we're supposed to guess on
	if (has_null_coltype)
	{
		if (!Eidos_RegexWorks())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_grep): This build of Eidos does not have a working <regex> library, due to a bug in the underlying C++ standard library provided by the system.  Calls to readCSV() that require guessing the type of a column (which uses regex) are therefore not allowed.  This problem might be resolved by updating your compiler or toolchain, or by upgrading to a more recent version of your operating system." << EidosTerminate(nullptr);
	
		std::regex integer_regex("[+-]?[0-9]+", std::regex_constants::ECMAScript);
		std::regex float_regex(std::string("[+-]?[0-9]+(\\") + std::string(1, dec) + std::string("[0-9]*)?([eE][+-]?[0-9]+)?"), std::regex_constants::ECMAScript);
		
		for (int col_index = 0; col_index < ncols; ++col_index)
		{
			if (coltypes[col_index] == EidosValueType::kValueNULL)
			{
				// Try EidosValueType::kValueLogical first; candidate values are "T", "TRUE", "true", "F", "FALSE", or "false", case-sensitive
				EidosValueType coltype = EidosValueType::kValueLogical;
				
				if (coltype == EidosValueType::kValueLogical)
				{
					for (const auto &row : rows)
					{
						const std::string &row_value = row[col_index];
						
						if ((row_value != "T") && (row_value != "TRUE") && (row_value != "true") && (row_value != "F") && (row_value != "FALSE") && (row_value != "false"))
						{
							coltype = EidosValueType::kValueInt;	// try integer next
							break;
						}
					}
				}
				
				if (coltype == EidosValueType::kValueInt)
				{
					for (const auto &row : rows)
					{
						const std::string &row_value = row[col_index];
						
						if (!std::regex_match(row_value, integer_regex))
						{
							coltype = EidosValueType::kValueFloat;	// try float next
							break;
						}
					}
				}
				
				if (coltype == EidosValueType::kValueFloat)
				{
					for (const auto &row : rows)
					{
						const std::string &row_value = row[col_index];
						
						if (Eidos_string_equalsCaseInsensitive(row_value, "NAN") ||
							Eidos_string_equalsCaseInsensitive(row_value, "INF") ||
							Eidos_string_equalsCaseInsensitive(row_value, "INFINITY") ||
							Eidos_string_equalsCaseInsensitive(row_value, "-INF") ||
							Eidos_string_equalsCaseInsensitive(row_value, "-INFINITY") ||
							Eidos_string_equalsCaseInsensitive(row_value, "+INF") ||
							Eidos_string_equalsCaseInsensitive(row_value, "+INFINITY"))
							continue;
						
						if (!std::regex_match(row_value, float_regex))
						{
							coltype = EidosValueType::kValueString;	// string is the fallback
							break;
						}
					}
				}
				
				coltypes[col_index] = coltype;
			}
		}
	}
	
	// Make the DataFrame to return
	EidosValue_SP result_SP(nullptr);
	EidosDataFrame *objectElement = new EidosDataFrame();
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDataFrame_Class));
	
	objectElement->Release();	// objectElement is now retained by result_SP, so we can release it
	
	// Put the row data into the DataFrame, column by column
	int nrows = (int)rows.size();
	
	for (int col_index = 0; col_index < ncols; ++col_index)
	{
		EidosValueType coltype = coltypes[col_index];
		
		// skip columns if requested
		if (coltype == EidosValueType::kValueVOID)
			continue;
		
		if (coltype == EidosValueType::kValueNULL)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): (internal error) column type was not guessed." << EidosTerminate(nullptr);
		
		EidosValue_SP column_values;
		
		if (coltype == EidosValueType::kValueLogical)
		{
			EidosValue_Logical *logical_column = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(nrows);
			column_values = EidosValue_SP(logical_column);
			
			for (int row_index = 0; row_index < nrows; ++row_index)
			{
				const std::string &row_value = rows[row_index][col_index];
				
				if ((row_value == "T") || (row_value == "TRUE") || (row_value == "true"))
					logical_column->set_logical_no_check(true, row_index);
				else if ((row_value == "F") || (row_value == "FALSE") || (row_value == "false"))
					logical_column->set_logical_no_check(false, row_index);
				else
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): (internal error) unexpected value '" << row_value << "' in logical column." << EidosTerminate(nullptr);
			}
		}
		else if (coltype == EidosValueType::kValueInt)
		{
			EidosValue_Int_vector *integer_column = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(nrows);
			column_values = EidosValue_SP(integer_column);
			
			for (int row_index = 0; row_index < nrows; ++row_index)
			{
				const std::string &row_value = rows[row_index][col_index];
				const char *row_cstr = row_value.c_str();
				char *last_used_char = nullptr;
				
				errno = 0;
				int64_t int_value = strtoll(row_cstr, &last_used_char, 10);
				
				if (errno || (last_used_char == row_cstr))
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): value '" << row_value << "' could not be represented as an integer (strtoll conversion error)." << EidosTerminate(nullptr);
				
				integer_column->set_int_no_check(int_value, row_index);
			}
		}
		else if (coltype == EidosValueType::kValueFloat)
		{
			EidosValue_Float_vector *float_column = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(nrows);
			column_values = EidosValue_SP(float_column);
			
			for (int row_index = 0; row_index < nrows; ++row_index)
			{
				std::string &row_value = rows[row_index][col_index];	// non-const here so we can fix the decimal separator
				double float_value;
				
				if (Eidos_string_equalsCaseInsensitive(row_value, "NAN"))
					float_value = std::numeric_limits<double>::quiet_NaN();
				else if (Eidos_string_equalsCaseInsensitive(row_value, "INF") ||
					Eidos_string_equalsCaseInsensitive(row_value, "INFINITY") ||
					Eidos_string_equalsCaseInsensitive(row_value, "+INF") ||
					Eidos_string_equalsCaseInsensitive(row_value, "+INFINITY"))
					float_value = std::numeric_limits<double>::infinity();
				else if (Eidos_string_equalsCaseInsensitive(row_value, "-INF") ||
					Eidos_string_equalsCaseInsensitive(row_value, "-INFINITY"))
					float_value = -std::numeric_limits<double>::infinity();
				else
				{
					if (dec != '.')
					{
						// We are in the C locale, so strtod() expects a '.' decimal separator.
						size_t dec_pos = row_value.find(dec);
						
						if (dec_pos != std::string::npos)
							row_value.replace(dec_pos, 1, 1, '.');
					}
					
					const char *row_cstr = row_value.c_str();
					char *last_used_char = nullptr;
					
					errno = 0;
					float_value = strtod(row_cstr, &last_used_char);
					
					if (errno || (last_used_char == row_cstr))
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): value '" << row_value << "' could not be represented as a float (strtod conversion error)." << EidosTerminate(nullptr);
				}
				
				float_column->set_float_no_check(float_value, row_index);
			}
		}
		else if (coltype == EidosValueType::kValueString)
		{
			EidosValue_String_vector *string_column = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(nrows);
			column_values = EidosValue_SP(string_column);
			
			for (int row_index = 0; row_index < nrows; ++row_index)
			{
				const std::string &row_value = rows[row_index][col_index];
				
				string_column->PushString(row_value);
			}
		}
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_readCSV): (internal error) unrecognized column type." << EidosTerminate(nullptr);
		
		objectElement->SetKeyValue_StringKeys(columnNames[col_index], column_values);
	}
	
	objectElement->ContentsChanged("readCSV()");
	
	return result_SP;
}


//
//	EidosDataFrame_Class
//
#pragma mark -
#pragma mark EidosDataFrame_Class
#pragma mark -

EidosClass *gEidosDataFrame_Class = nullptr;

const std::vector<EidosPropertySignature_CSP> *EidosDataFrame_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosDataFrame_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_colNames,			true,	kEidosValueMaskString)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_dim,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_ncol,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_nrow,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosDataFrame_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosDataFrame_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_asMatrix, kEidosValueMaskAny)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_cbind, kEidosValueMaskVOID))->AddObject("source", nullptr)->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_rbind, kEidosValueMaskVOID))->AddObject("source", nullptr)->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_subset, kEidosValueMaskAny))
			->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskOptional, "rows", nullptr, gStaticEidosValueNULL)
			->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptional, "cols", nullptr, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_subsetColumns, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDataFrame_Class))->AddArg(kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskString, "index", nullptr));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_subsetRows, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDataFrame_Class))->AddArg(kEidosValueMaskLogical | kEidosValueMaskInt, "index", nullptr)->AddLogical_OS("drop", gStaticEidosValue_LogicalF));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const std::vector<EidosFunctionSignature_CSP> *EidosDataFrame_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosDataFrame_Class::Functions(): not warmed up");
		
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_DataFrame, Eidos_Instantiate_EidosDataFrame, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDataFrame_Class))->AddEllipsis());
		
		// I'm adding this here rather than in eidos_functions because it feels like a constructor, and thus belongs to the class,
		// and having the code for it in this source file rather than eidos_functions.cpp feels more cohesive and comprehensible.
		// Indeed, I can imagine the syntax shifting to DataFrame.newFromCSV() or some such, if Eidos ever makes class objects public.
		// It is documented in EidosHelpFunctions for now, though, since unless it is *actually* a constructor it would be confusing.
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("readCSV", Eidos_ExecuteFunction_readCSV, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDataFrame_Class))->AddString_S("filePath")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskString | kEidosValueMaskOptional, "colNames", nullptr, gStaticEidosValue_LogicalT)->AddString_OSN("colTypes", gStaticEidosValueNULL)->AddString_OS("sep", gStaticEidosValue_StringComma)->AddString_OS("quote", gStaticEidosValue_StringDoubleQuote)->AddString_OS("dec", gStaticEidosValue_StringPeriod)->AddString_OS("comment", gStaticEidosValue_StringEmpty));
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}














