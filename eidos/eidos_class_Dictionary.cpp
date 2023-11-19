//
//  eidos_class_Dictionary.cpp
//  Eidos
//
//  Created by Ben Haller on 10/12/20.
//  Copyright (c) 2020-2023 Philipp Messer.  All rights reserved.
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

#include "eidos_class_Dictionary.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"
#include "eidos_value.h"
#include "eidos_functions.h"
#include "json.hpp"

#include <utility>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <string>


int64_t gEidos_DictionaryNonRetainReleaseReferenceCounter = 0;


//
// EidosDictionaryUnretained
//
#pragma mark -
#pragma mark EidosDictionaryUnretained
#pragma mark -

EidosDictionaryUnretained::~EidosDictionaryUnretained(void)
{
	// maintain the global counter of dictionaries that contain non-retain-released objects
	if (state_ptr_)
	{
		uint8_t contains = ((EidosDictionaryState_StringKeys *)state_ptr_)->contains_non_retain_release_objects_;
		
		if (contains)
		{
#pragma omp critical (EidosDictionary_Internal)
			{
				gEidos_DictionaryNonRetainReleaseReferenceCounter--;
				
				if (gEidos_DictionaryNonRetainReleaseReferenceCounter < 0)
					EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::~EidosDictionaryUnretained): (internal error) gEidos_DictionaryNonRetainReleaseReferenceCounter is negative" << EidosTerminate(nullptr);
				
				//std::cerr << "=== dictionary " << this << " INC   ->   " << gEidos_DictionaryNonRetainReleaseReferenceCounter << std::endl;
			}
		}
	}
	
	// delete our internal state
	if (state_ptr_)
	{
		if (KeysAreStrings())
			delete (EidosDictionaryState_StringKeys *)state_ptr_;
		else
			delete (EidosDictionaryState_IntegerKeys *)state_ptr_;
		
		state_ptr_ = nullptr;
	}
}

void EidosDictionaryUnretained::Raise_UsesStringKeys() const
{
	EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Raise_UsesStringKeys): the target Dictionary object is configured to use string keys; an integer key cannot be used." << EidosTerminate(nullptr);
}

void EidosDictionaryUnretained::Raise_UsesIntegerKeys() const
{
	EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Raise_UsesIntegerKeys): the target Dictionary object is configured to use integer keys; a string key cannot be used." << EidosTerminate(nullptr);
}

std::vector<std::string> EidosDictionaryUnretained::SortedKeys_StringKeys(void) const
{
	// Dictionary provides keys in sorted order, which is done on demand.  Many use cases
	// never need the keys at all, or only once, so we do not maintain the keys sorted.
	const EidosDictionaryHashTable_StringKeys *dict_symbols = DictionarySymbols_StringKeys();
	
	std::vector<std::string> string_keys;
	
	if (dict_symbols)
	{
		for (auto &iter : *dict_symbols)
			string_keys.push_back(iter.first);
		
		std::sort(string_keys.begin(), string_keys.end());
	}
	
	return string_keys;
}

std::vector<int64_t> EidosDictionaryUnretained::SortedKeys_IntegerKeys(void) const
{
	// Dictionary provides keys in sorted order, which is done on demand.  Many use cases
	// never need the keys at all, or only once, so we do not maintain the keys sorted.
	const EidosDictionaryHashTable_IntegerKeys *dict_symbols = DictionarySymbols_IntegerKeys();
	
	std::vector<int64_t> integer_keys;
	
	if (dict_symbols)
	{
		for (auto &iter : *dict_symbols)
			integer_keys.push_back(iter.first);
		
		std::sort(integer_keys.begin(), integer_keys.end());
	}
	
	return integer_keys;
}

void EidosDictionaryUnretained::KeyAddedToDictionary_StringKeys(__attribute__((unused)) const std::string &p_key)
{
	// Dictionary does not need to do any extra work when a key is added; this is for subclasses
}

void EidosDictionaryUnretained::KeyAddedToDictionary_IntegerKeys(__attribute__((unused)) int64_t p_key)
{
	// Dictionary does not need to do any extra work when a key is added; this is for subclasses
}

void EidosDictionaryUnretained::KeyRemovedFromDictionary_StringKeys(__attribute__((unused)) const std::string &p_key)
{
	// Dictionary does not need to do any extra work when a key is removed; this is for subclasses
}

void EidosDictionaryUnretained::KeyRemovedFromDictionary_IntegerKeys(__attribute__((unused)) int64_t p_key)
{
	// Dictionary does not need to do any extra work when a key is removed; this is for subclasses
}

void EidosDictionaryUnretained::AllKeysRemoved(void)
{
	// Dictionary does not need to do any extra work when a key is removed; this is for subclasses
}

void EidosDictionaryUnretained::ContentsChanged(const std::string &p_operation_name)
{
	// Here we keep track of whether or not we contain any non-retain-release objects
	// Note that we don't need to scan down into contained Dictionary objects;
	// we retain them, and they will do their own checks of their own contents
	if (!state_ptr_)
		return;
	
	uint8_t old_contains = ((EidosDictionaryState_StringKeys *)state_ptr_)->contains_non_retain_release_objects_;
	uint8_t new_contains = false;
	
	if (KeysAreStrings())
	{
		const EidosDictionaryHashTable_StringKeys *dict_symbols = DictionarySymbols_StringKeys();
		
		for (auto &iter : *dict_symbols)
		{
			EidosValue *value = iter.second.get();
			
			if ((value->Type() == EidosValueType::kValueObject) && (value->Count() > 0))
			{
				const EidosClass *value_class = ((EidosValue_Object *)value)->Class();
				
				if (!value_class->UsesRetainRelease())
				{
					new_contains = true;
					break;
				}
			}
		}
	}
	else
	{
		const EidosDictionaryHashTable_IntegerKeys *dict_symbols = DictionarySymbols_IntegerKeys();
		
		for (auto &iter : *dict_symbols)
		{
			EidosValue *value = iter.second.get();
			
			if ((value->Type() == EidosValueType::kValueObject) && (value->Count() > 0))
			{
				const EidosClass *value_class = ((EidosValue_Object *)value)->Class();
				
				if (!value_class->UsesRetainRelease())
				{
					new_contains = true;
					break;
				}
			}
		}
	}
	
	if (old_contains != new_contains)
	{
		((EidosDictionaryState_StringKeys *)state_ptr_)->contains_non_retain_release_objects_ = new_contains;
		
#pragma omp critical (EidosDictionary_Internal)
		{
			if (new_contains)
			{
				gEidos_DictionaryNonRetainReleaseReferenceCounter++;
				
				//std::cerr << "=== dictionary " << this << " INC   ->   " << gEidos_DictionaryNonRetainReleaseReferenceCounter << std::endl;
			}
			else
			{
				gEidos_DictionaryNonRetainReleaseReferenceCounter--;
				
				if (gEidos_DictionaryNonRetainReleaseReferenceCounter < 0)
					EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ContentsChanged): (internal error) gEidos_DictionaryNonRetainReleaseReferenceCounter is negative in operation " << p_operation_name << EidosTerminate(nullptr);
				
				//std::cerr << "   === dictionary " << this << " DEC   ->   " << gEidos_DictionaryNonRetainReleaseReferenceCounter << std::endl;
			}
		}
	}
}

std::string EidosDictionaryUnretained::Serialization_SLiM(void) const
{
	// Loop through our key-value pairs and serialize them
	if (!state_ptr_)
		return "";
	
	std::ostringstream ss;
	
	if (KeysAreStrings())
	{
		// Serialize a dictionary with string keys
		const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
		
		// We want to output our keys in the same order as allKeys, so we just use AllKeys()
		EidosValue_SP all_keys = AllKeys();
		EidosValue_String_vector *string_vec = dynamic_cast<EidosValue_String_vector *>(all_keys.get());
		
		if (!string_vec)
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization_SLiM): (internal error) allKeys did not return a string vector." << EidosTerminate(nullptr);
		
		const std::vector<std::string> *all_key_strings = string_vec->StringVector();
		
		for (const std::string &key : *all_key_strings)
		{
			// emit the key; note that we now always quote stringkeys, to distinguish them from the integer-key case easily
			ss << Eidos_string_escaped(key, EidosStringQuoting::kDoubleQuotes) << "=";
			
			// emit the value
			auto hash_iter = symbols->find(key);
			
			if (hash_iter == symbols->end())
			{
				// We assume that this is not an internal error, but is instead LogFile with a column that is NA;
				// it returns all of its column names for AllKeys() even if they have NA as a value, so we play along
				ss << "NA;";
			}
			else
			{
				ss << *(hash_iter->second) << ";";
			}
		}
	}
	else	// KeysAreIntegers()
	{
		// Serialize a dictionary with integer keys
		const EidosDictionaryHashTable_IntegerKeys *symbols = DictionarySymbols_IntegerKeys();
		
		// We want to output our keys in the same order as allKeys, so we just use AllKeys()
		EidosValue_SP all_keys = AllKeys();
		EidosValue_Int_vector *integer_vec = dynamic_cast<EidosValue_Int_vector *>(all_keys.get());
		
		if (!integer_vec)
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization_SLiM): (internal error) allKeys did not return an integer vector." << EidosTerminate(nullptr);
		
		const int64_t *all_key_integers = integer_vec->data();
		int all_keys_count = integer_vec->Count();
		
		for (int i = 0; i < all_keys_count; ++i)
		{
			int64_t key = all_key_integers[i];
			
			// emit the key
			ss << key << "=";
			
			// emit the value
			auto hash_iter = symbols->find(key);
			
			if (hash_iter == symbols->end())
			{
				// We assume that this is not an internal error, but is instead LogFile with a column that is NA;
				// it returns all of its column names for AllKeys() even if they have NA as a value, so we play along
				ss << "NA;";
			}
			else
			{
				ss << *(hash_iter->second) << ";";
			}
		}
	}
	
	return ss.str();
}

EidosValue_SP EidosDictionaryUnretained::Serialization_CSV(const std::string &p_delimiter) const
{
	if (!state_ptr_)
		return gStaticEidosValue_StringEmpty;
	
	// Determine the longest column, and also generate our header string
	bool keysAreStrings = KeysAreStrings();
	const EidosDictionaryHashTable_StringKeys *symbols_string = nullptr;
	const EidosDictionaryHashTable_IntegerKeys *symbols_integer = nullptr;
	std::vector<std::string> keys_string;
	std::vector<int64_t> keys_integer;
	size_t keys_count;
	int longest_col = 0;
	
	if (keysAreStrings)
	{
		symbols_string = DictionarySymbols_StringKeys();
		keys_string = SortedKeys_StringKeys();
		keys_count = keys_string.size();
		
		for (auto const &kv_pair : *symbols_string)
			longest_col = std::max(longest_col, kv_pair.second->Count());
	}
	else
	{
		symbols_integer = DictionarySymbols_IntegerKeys();
		keys_integer = SortedKeys_IntegerKeys();
		keys_count = keys_integer.size();
		
		for (auto const &kv_pair : *symbols_integer)
			longest_col = std::max(longest_col, kv_pair.second->Count());
	}
	
	// Make a string vector big enough for everything
	int result_count = longest_col + 1;		// room for the header
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(result_count);
	EidosValue_SP result_SP(string_result);
	
	// Generate the header
	{
		std::ostringstream ss;
		bool first = true;
		
		for (size_t i = 0; i < keys_count; ++i)
		{
			if (!first)
				ss << p_delimiter;
			first = false;
			
			if (keysAreStrings)
				ss << Eidos_string_escaped_CSV(keys_string[i]);
			else
				ss << keys_integer[i];
		}
		
		string_result->PushString(ss.str());
	}
	
	// Generate the rows
	for (int row_index = 0; row_index < longest_col; ++row_index)
	{
		std::ostringstream ss;
		bool first = true;
		
		for (size_t i = 0; i < keys_count; ++i)
		{
			if (!first)
				ss << p_delimiter;
			first = false;
			
			EidosValue *value = nullptr;
			
			if (keysAreStrings)
			{
				auto col_iter = symbols_string->find(keys_string[i]);
				
				if (col_iter != symbols_string->end())
					value = col_iter->second.get();
			}
			else
			{
				auto col_iter = symbols_integer->find(keys_integer[i]);
				
				if (col_iter != symbols_integer->end())
					value = col_iter->second.get();
			}
			
			if (!value)
				EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization_CSV): (internal error) column not found." << EidosTerminate(nullptr);
			
			int value_count = value->Count();
			
			// if a column has no value for this row, we just skip it (but with the delimiter we already emitted)
			if (row_index < value_count)
			{
				switch (value->Type())
				{
					case EidosValueType::kValueVOID:
						EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization_CSV): cannot serialize values of type void to CSV/TSV." << EidosTerminate(nullptr);
					case EidosValueType::kValueNULL:
						EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization_CSV): cannot serialize values of type NULL to CSV/TSV." << EidosTerminate(nullptr);
					case EidosValueType::kValueObject:
						EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization_CSV): cannot serialize values of type object to CSV/TSV." << EidosTerminate(nullptr);
						
					case EidosValueType::kValueLogical: ss << (value->LogicalAtIndex(row_index, nullptr) ? "TRUE" : "FALSE"); break;
					case EidosValueType::kValueInt: ss << value->IntAtIndex(row_index, nullptr); break;
					case EidosValueType::kValueFloat:
					{
						int old_precision = gEidosFloatOutputPrecision;
						gEidosFloatOutputPrecision = EIDOS_DBL_DIGS - 2;	// try to avoid ugly values that exhibit the precision limits
						ss << EidosStringForFloat(value->FloatAtIndex(row_index, nullptr));
						gEidosFloatOutputPrecision = old_precision;
						break;
					}
					case EidosValueType::kValueString:
					{
						ss << Eidos_string_escaped_CSV(value->StringAtIndex(row_index, nullptr));
						break;
					}
				}
			}
		}
		
		string_result->PushString(ss.str());
	}
	
	return result_SP;
}

nlohmann::json EidosDictionaryUnretained::JSONRepresentation(void) const
{
	if (!KeysAreStrings())
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::JSONRepresentation): Dictionary does not support JSON serialization with integer keys, because JSON itself does not support integer keys." << EidosTerminate(nullptr);
	
	const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
	nlohmann::json json_object = nlohmann::json::object();
	
	// We want to output our keys in the same order as allKeys, so we just use AllKeys()
	EidosValue_SP all_keys = AllKeys();
	EidosValue_String_vector *string_vec = dynamic_cast<EidosValue_String_vector *>(all_keys.get());
	
	if (!string_vec)
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::JSONRepresentation): (internal error) allKeys did not return a string vector." << EidosTerminate(nullptr);
	
	const std::vector<std::string> *all_key_strings = string_vec->StringVector();
	
	for (const std::string &key : *all_key_strings)
	{
		// get the value
		auto hash_iter = symbols->find(key);
		
		if (hash_iter == symbols->end())
		{
			// We assume that this is not an internal error, but is instead LogFile with a column that is NA;
			// it returns all of its column names for AllKeys() even if they have NA as a value, so we play along
			json_object[key] = nullptr;
		}
		else
		{
			EidosValue *value = hash_iter->second.get();
			
			json_object[key] = value->JSONRepresentation();
		}
	}
	
	return json_object;
}

void EidosDictionaryUnretained::SetKeyValue_StringKeys(const std::string &key, EidosValue_SP value)
{
	AssertKeysAreStrings();
	
	EidosDictionaryState_StringKeys *state_ptr = (EidosDictionaryState_StringKeys *)state_ptr_;
	EidosValueType value_type = value->Type();
	
	// BCH 2/12/2023: We now allow objects to be put into dictionaries whether they use retain-release or not,
	// so we no longer check for that here.  See EidosDictionaryUnretained::ContentsChanged() and search for
	// contains_non_retain_release_objects_ and gEidos_DictionaryNonRetainReleaseReferenceCounter.
	
	if (value_type == EidosValueType::kValueNULL)
	{
		// Setting a key to NULL removes it from the map; if we have no state, that is a no-op
		if (state_ptr)
		{
			state_ptr->dictionary_symbols_.erase(key);
			KeyRemovedFromDictionary_StringKeys(key);
		}
	}
	else
	{
		if (!state_ptr)
		{
			state_ptr = new EidosDictionaryState_StringKeys();
			state_ptr->keys_are_integers_ = false;
			state_ptr_ = state_ptr;
		}
		
		// Copy if necessary; see ExecuteMethod_Accelerated_setValue() for comments
		if ((value->UseCount() != 1) || value->Invisible())
			value = value->CopyValues();
		
		state_ptr->dictionary_symbols_[key] = value;
		
		// Add it to our sorted keys
		KeyAddedToDictionary_StringKeys(key);
	}
}

void EidosDictionaryUnretained::SetKeyValue_IntegerKeys(int64_t key, EidosValue_SP value)
{
	AssertKeysAreIntegers();
	
	EidosDictionaryState_IntegerKeys *state_ptr = (EidosDictionaryState_IntegerKeys *)state_ptr_;
	EidosValueType value_type = value->Type();
	
	// BCH 2/12/2023: We now allow objects to be put into dictionaries whether they use retain-release or not,
	// so we no longer check for that here.  See EidosDictionaryUnretained::ContentsChanged() and search for
	// contains_non_retain_release_objects_ and gEidos_DictionaryNonRetainReleaseReferenceCounter.
	
	if (value_type == EidosValueType::kValueNULL)
	{
		// Setting a key to NULL removes it from the map; if we have no state, that is a no-op
		if (state_ptr)
		{
			state_ptr->dictionary_symbols_.erase(key);
			KeyRemovedFromDictionary_IntegerKeys(key);
		}
	}
	else
	{
		if (!state_ptr)
		{
			state_ptr = new EidosDictionaryState_IntegerKeys();
			state_ptr->keys_are_integers_ = true;
			state_ptr_ = state_ptr;
		}
		
		// Copy if necessary; see ExecuteMethod_Accelerated_setValue() for comments
		if ((value->UseCount() != 1) || value->Invisible())
			value = value->CopyValues();
		
		state_ptr->dictionary_symbols_[key] = value;
		
		// Add it to our sorted keys
		KeyAddedToDictionary_IntegerKeys(key);
	}
}

EidosValue_SP EidosDictionaryUnretained::GetValueForKey_StringKeys(const std::string &key)
{
	// see also EidosDictionaryUnretained::ExecuteMethod_getValue()
	AssertKeysAreStrings();
	
	const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
	
	if (!symbols)
		return gStaticEidosValueNULL;
	
	auto found_iter = symbols->find(key);
	
	if (found_iter == symbols->end())
	{
		return gStaticEidosValueNULL;
	}
	else
	{
		return found_iter->second;
	}
}

EidosValue_SP EidosDictionaryUnretained::GetValueForKey_IntegerKeys(int64_t key)
{
	// see also EidosDictionaryUnretained::ExecuteMethod_getValue()
	AssertKeysAreIntegers();
	
	const EidosDictionaryHashTable_IntegerKeys *symbols = DictionarySymbols_IntegerKeys();
	
	if (!symbols)
		return gStaticEidosValueNULL;
	
	auto found_iter = symbols->find(key);
	
	if (found_iter == symbols->end())
	{
		return gStaticEidosValueNULL;
	}
	else
	{
		return found_iter->second;
	}
}

EidosValue_SP EidosDictionaryUnretained::AllKeys(void) const
{
	if (KeysAreStrings())
	{
		const std::vector<std::string> keys = SortedKeys_StringKeys();
		int key_count = (int)keys.size();
		
		if (key_count == 0)
			return gStaticEidosValue_String_ZeroVec;
		
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(key_count);
		
		for (const std::string &key : keys)
			string_result->PushString(key);
		
		return EidosValue_SP(string_result);
	}
	else
	{
		const std::vector<int64_t> keys = SortedKeys_IntegerKeys();
		int key_count = (int)keys.size();
		
		if (key_count == 0)
			return gStaticEidosValue_Integer_ZeroVec;
		
		EidosValue_Int_vector *integer_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve(key_count);
		
		for (int64_t key : keys)
			integer_result->push_int_no_check(key);
		
		return EidosValue_SP(integer_result);
	}
}

void EidosDictionaryUnretained::AddKeysAndValuesFrom(EidosDictionaryUnretained *p_source, bool p_allow_replace /* = true */)
{
	// Loop through the key-value pairs of source and add them
	if (!p_source->state_ptr_)
		return;
	
	if (p_source->KeysAreStrings())
	{
		AssertKeysAreStrings();
		
		const EidosDictionaryHashTable_StringKeys *source_symbols = p_source->DictionarySymbols_StringKeys();
		const std::vector<std::string> source_keys = p_source->SortedKeys_StringKeys();
		
		if (source_symbols && source_symbols->size())
		{
			EidosDictionaryState_StringKeys *state_ptr = (EidosDictionaryState_StringKeys *)state_ptr_;
			
			if (!state_ptr)
			{
				state_ptr = new EidosDictionaryState_StringKeys();
				state_ptr->keys_are_integers_ = false;
				state_ptr_ = state_ptr;
			}
			
			for (const std::string &key : source_keys)
			{
				auto kv_pair = source_symbols->find(key);
				const EidosValue_SP &value = kv_pair->second;
				
				if (!p_allow_replace)
				{
					// This is for DataFrame's -cbind(), which does not want to replace existing columns
					auto check_iter = state_ptr->dictionary_symbols_.find(key);
					
					if (check_iter != state_ptr->dictionary_symbols_.end())
						EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AddKeysAndValuesFrom): a column named '" << key << "' already exists." << EidosTerminate(nullptr);
				}
				
				// always copy values here; unlike setValue(), we know the value is in use elsewhere
				state_ptr->dictionary_symbols_[key] = value->CopyValues();
				
				KeyAddedToDictionary_StringKeys(key);
			}
		}
	}
	else	// p_source->KeysAreIntegers()
	{
		AssertKeysAreIntegers();
		
		const EidosDictionaryHashTable_IntegerKeys *source_symbols = p_source->DictionarySymbols_IntegerKeys();
		const std::vector<int64_t> source_keys = p_source->SortedKeys_IntegerKeys();
		
		if (source_symbols && source_symbols->size())
		{
			EidosDictionaryState_IntegerKeys *state_ptr = (EidosDictionaryState_IntegerKeys *)state_ptr_;
			
			if (!state_ptr)
			{
				state_ptr = new EidosDictionaryState_IntegerKeys();
				state_ptr->keys_are_integers_ = true;
				state_ptr_ = state_ptr;
			}
			
			for (int64_t key : source_keys)
			{
				auto kv_pair = source_symbols->find(key);
				const EidosValue_SP &value = kv_pair->second;
				
				if (!p_allow_replace)
				{
					// This is for DataFrame's -cbind(), which does not want to replace existing columns
					auto check_iter = state_ptr->dictionary_symbols_.find(key);
					
					if (check_iter != state_ptr->dictionary_symbols_.end())
						EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AddKeysAndValuesFrom): a column named " << key << " already exists." << EidosTerminate(nullptr);
				}
				
				// always copy values here; unlike setValue(), we know the value is in use elsewhere
				state_ptr->dictionary_symbols_[key] = value->CopyValues();
				
				KeyAddedToDictionary_IntegerKeys(key);
			}
		}
	}
}

void EidosDictionaryUnretained::AppendKeysAndValuesFrom(EidosDictionaryUnretained *p_source, bool p_require_column_match /* = false */)
{
	// Check for x.appendKeysAndValuesFrom(x); I'm not sure that this would confuse the iterator below, but it seems like a bad idea
	if (p_source == this)
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AppendKeysAndValuesFrom): cannot append a Dictionary to itself." << EidosTerminate(nullptr);
	
	if (!p_source->state_ptr_)
		return;
	
	if (p_source->KeysAreStrings())
	{
		AssertKeysAreStrings();
		
		// Loop through the key-value pairs of source and add them
		const EidosDictionaryHashTable_StringKeys *source_symbols = p_source->DictionarySymbols_StringKeys();
		
		if (source_symbols && source_symbols->size())
		{
			EidosDictionaryState_StringKeys *state_ptr = (EidosDictionaryState_StringKeys *)state_ptr_;
			
			if (!state_ptr_)
			{
				state_ptr = new EidosDictionaryState_StringKeys();
				state_ptr->keys_are_integers_ = false;
				state_ptr_ = state_ptr;
			}
			
			const std::vector<std::string> source_keys = p_source->SortedKeys_StringKeys();
			const std::vector<std::string> keys = SortedKeys_StringKeys();
			
			// This is for DataFrame's rbind(), which wants columns to match exactly (if any columns are already there)
			if (p_require_column_match && keys.size() && (keys != source_keys))
				EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AppendKeysAndValuesFrom): the columns of the target do not match the columns of the dictionary being appended." << EidosTerminate(nullptr);
			
			for (const std::string &key : source_keys)
			{
				auto kv_pair = source_symbols->find(key);
				const EidosValue_SP &keyvalue = kv_pair->second;
				
				auto found_iter = state_ptr->dictionary_symbols_.find(key);
				
				if (found_iter == state_ptr->dictionary_symbols_.end())
				{
					// always copy values here; unlike setValue(), we know the value is in use elsewhere
					state_ptr->dictionary_symbols_[key] = keyvalue->CopyValues();
					
					KeyAddedToDictionary_StringKeys(key);
				}
				else
				{
					// we already have a value; append (which could be done in place, since we have sole ownership of our values,
					// but at present we're not that smart, and it's complicated since our existing value might be a singleton)
					EidosValue_SP existing_value = found_iter->second;
					std::vector<EidosValue_SP> cat_args = {existing_value, keyvalue};
					EidosValue_SP appended_value = ConcatenateEidosValues(cat_args, false, false);
					
					state_ptr->dictionary_symbols_[key] = appended_value;
				}
			}
		}
	}
	else
	{
		AssertKeysAreIntegers();
		
		// Loop through the key-value pairs of source and add them
		const EidosDictionaryHashTable_IntegerKeys *source_symbols = p_source->DictionarySymbols_IntegerKeys();
		
		if (source_symbols && source_symbols->size())
		{
			EidosDictionaryState_IntegerKeys *state_ptr = (EidosDictionaryState_IntegerKeys *)state_ptr_;
			
			if (!state_ptr_)
			{
				state_ptr = new EidosDictionaryState_IntegerKeys();
				state_ptr->keys_are_integers_ = true;
				state_ptr_ = state_ptr;
			}
			
			const std::vector<int64_t> source_keys = p_source->SortedKeys_IntegerKeys();
			const std::vector<int64_t> keys = SortedKeys_IntegerKeys();
			
			// This is for DataFrame's rbind(), which wants columns to match exactly (if any columns are already there)
			if (p_require_column_match && keys.size() && (keys != source_keys))
				EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AppendKeysAndValuesFrom): the columns of the target do not match the columns of the dictionary being appended." << EidosTerminate(nullptr);
			
			for (int64_t key : source_keys)
			{
				auto kv_pair = source_symbols->find(key);
				const EidosValue_SP &keyvalue = kv_pair->second;
				
				auto found_iter = state_ptr->dictionary_symbols_.find(key);
				
				if (found_iter == state_ptr->dictionary_symbols_.end())
				{
					// always copy values here; unlike setValue(), we know the value is in use elsewhere
					state_ptr->dictionary_symbols_[key] = keyvalue->CopyValues();
					
					KeyAddedToDictionary_IntegerKeys(key);
				}
				else
				{
					// we already have a value; append (which could be done in place, since we have sole ownership of our values,
					// but at present we're not that smart, and it's complicated since our existing value might be a singleton)
					EidosValue_SP existing_value = found_iter->second;
					std::vector<EidosValue_SP> cat_args = {existing_value, keyvalue};
					EidosValue_SP appended_value = ConcatenateEidosValues(cat_args, false, false);
					
					state_ptr->dictionary_symbols_[key] = appended_value;
				}
			}
		}
	}
}

void EidosDictionaryUnretained::AddJSONFrom(nlohmann::json &json)
{
	if (!KeysAreStrings())
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AddJSONFrom): JSON objects can only be added to a dictionary that uses string keys, since JSON uses string keys." << EidosTerminate(nullptr);
	
	EidosDictionaryState_StringKeys *state_ptr = (EidosDictionaryState_StringKeys *)state_ptr_;
	
	// null at the top level indicates an empty dictionary, so we just return
	if (!json.is_null())
	{
		// otherwise, we require the top level to be a JSON "object" - a key-value dictionary
		if (json.is_object())
		{
			// iterate over the key-value pairs in the "object"
			for (auto &element : json.items())
			{
				if (!state_ptr)
				{
					state_ptr = new EidosDictionaryState_StringKeys();
					state_ptr->keys_are_integers_ = false;
					state_ptr_ = state_ptr;
				}
				
				std::string key = element.key();
				auto &value = element.value();
				
				//std::cout << element.key() << " : " << element.value() << std::endl;
				
				// decode the value and assign it to the key
				if (value.is_null())
				{
					EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
					state_ptr->dictionary_symbols_[key] = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
					dictionary->Release();		// now retained by state_ptr->dictionary_symbols_
				}
				else if (value.is_boolean())
				{
					bool boolean_value = value;
					state_ptr->dictionary_symbols_[key] = (boolean_value ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
				}
				else if (value.is_string())
				{
					const std::string &string_value = value;
					state_ptr->dictionary_symbols_[key] = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string_value));
				}
				else if (value.is_number_integer() || value.is_number_unsigned())
				{
					int64_t int_value = value;
					state_ptr->dictionary_symbols_[key] = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(int_value));
				}
				else if (value.is_number_float())
				{
					double float_value = value;
					state_ptr->dictionary_symbols_[key] = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(float_value));
				}
				else if (value.is_object())
				{
					EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
					dictionary->AddJSONFrom(value);
					state_ptr->dictionary_symbols_[key] = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
					dictionary->Release();		// now retained by state_ptr->dictionary_symbols_
				}
				else if (value.is_array())
				{
					size_t array_count = value.size();
					
					if (array_count == 0)
					{
						// We don't what type the empty vector is; we assume integer
						// This means that empty vectors don't persist accurately through JSON; I see no solution
						state_ptr->dictionary_symbols_[key] = gStaticEidosValue_Integer_ZeroVec;
					}
					else
					{
						// figure out the type of element 0; we fold unsigned and integer together, and we fold null and object together
						nlohmann::json::value_t array_type = value[0].type();
						if (array_type == nlohmann::json::value_t::number_unsigned)
							array_type = nlohmann::json::value_t::number_integer;
						if (array_type == nlohmann::json::value_t::null)
							array_type = nlohmann::json::value_t::object;
						
						// confirm that all elements in the array have the same type, since Eidos vectors are on a single type
						for (size_t element_index = 0; element_index < array_count; ++element_index)
						{
							nlohmann::json::value_t element_type = value[element_index].type();
							if (element_type == nlohmann::json::value_t::number_unsigned)
								element_type = nlohmann::json::value_t::number_integer;
							if (element_type == nlohmann::json::value_t::null)
								element_type = nlohmann::json::value_t::object;
							
							if (element_type != array_type)
								EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AddJSONFrom): AddJSONFrom() requires that JSON arrays be of a single type, since Eidos vectors are of a single type." << EidosTerminate(nullptr);
						}
						
						// ok, all elements are of a single type, so let's create a vector of that type from the values in the array
						if (array_type == nlohmann::json::value_t::number_integer)
						{
							EidosValue_Int_vector *int_value = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(array_count);
							
							for (size_t element_index = 0; element_index < array_count; ++element_index)
							{
								int64_t int_element = value[element_index];
								int_value->set_int_no_check(int_element, element_index);
							}
							
							state_ptr->dictionary_symbols_[key] = EidosValue_SP(int_value);
						}
						else if (array_type == nlohmann::json::value_t::number_float)
						{
							EidosValue_Float_vector *float_value = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(array_count);
							
							for (size_t element_index = 0; element_index < array_count; ++element_index)
							{
								double float_element = value[element_index];
								float_value->set_float_no_check(float_element, element_index);
							}
							
							state_ptr->dictionary_symbols_[key] = EidosValue_SP(float_value);
						}
						else if (array_type == nlohmann::json::value_t::boolean)
						{
							EidosValue_Logical *logical_value = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(array_count);
							
							for (size_t element_index = 0; element_index < array_count; ++element_index)
							{
								bool boolean_element = value[element_index];
								logical_value->set_logical_no_check(boolean_element, element_index);
							}
							
							state_ptr->dictionary_symbols_[key] = EidosValue_SP(logical_value);
						}
						else if (array_type == nlohmann::json::value_t::string)
						{
							EidosValue_String_vector *string_value = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)array_count);
							
							for (size_t element_index = 0; element_index < array_count; ++element_index)
							{
								const std::string &string_element = value[element_index];
								string_value->PushString(string_element);
							}
							
							state_ptr->dictionary_symbols_[key] = EidosValue_SP(string_value);
						}
						else if (array_type == nlohmann::json::value_t::object)
						{
							EidosValue_Object_vector *object_value = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosDictionaryRetained_Class);
							
							for (size_t element_index = 0; element_index < array_count; ++element_index)
							{
								EidosDictionaryRetained *element_dictionary = new EidosDictionaryRetained();
								
								object_value->push_object_element_RR(element_dictionary);	// retain it in object_value
								element_dictionary->Release();
								
								if (value[element_index].type() == nlohmann::json::value_t::object)
									element_dictionary->AddJSONFrom(value[element_index]);
							}
							
							state_ptr->dictionary_symbols_[key] = EidosValue_SP(object_value);
						}
						else
						{
							EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AddJSONFrom): unsupported array value type '" << value[0].type_name() << "' in AddJSONFrom()." << EidosTerminate(nullptr);
						}
					}
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AddJSONFrom): unsupported value type '" << value.type_name() << "' in AddJSONFrom()." << EidosTerminate(nullptr);
				}
				
				KeyAddedToDictionary_StringKeys(key);
			}
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::AddJSONFrom): AddJSONFrom() can only parse JSON strings that represent a JSON 'object'; i.e., a dictionary of key-value pairs." << EidosTerminate(nullptr);
		}
	}
}


//
// EidosDictionaryUnretained Eidos support
//
#pragma mark -
#pragma mark EidosDictionaryUnretained Eidos support
#pragma mark -

const EidosClass *EidosDictionaryUnretained::Class(void) const
{
	return gEidosDictionaryUnretained_Class;
}

void EidosDictionaryUnretained::Print(std::ostream &p_ostream) const
{
	p_ostream << "{" << Serialization_SLiM() << "}";
}

EidosValue_SP EidosDictionaryUnretained::GetProperty(EidosGlobalStringID p_property_id)
{
#if DEBUG
	// Check for correctness before dispatching out; perhaps excessively cautious, but checks are good
	ContentsChanged("EidosDictionaryUnretained::GetProperty");
#endif
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gEidosID_allKeys:
			return AllKeys();
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue_SP EidosDictionaryUnretained::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#if DEBUG
	// Check for correctness before dispatching out; perhaps excessively cautious, but checks are good
	ContentsChanged("EidosDictionaryUnretained::ExecuteInstanceMethod");
#endif
	
	switch (p_method_id)
	{
		case gEidosID_addKeysAndValuesFrom:		return ExecuteMethod_addKeysAndValuesFrom(p_method_id, p_arguments, p_interpreter);
		case gEidosID_appendKeysAndValuesFrom:	return ExecuteMethod_appendKeysAndValuesFrom(p_method_id, p_arguments, p_interpreter);
		case gEidosID_clearKeysAndValues:		return ExecuteMethod_clearKeysAndValues(p_method_id, p_arguments, p_interpreter);
		case gEidosID_compactIndices:			return ExecuteMethod_compactIndices(p_method_id, p_arguments, p_interpreter);
		case gEidosID_getRowValues:				return ExecuteMethod_getRowValues(p_method_id, p_arguments, p_interpreter);
		case gEidosID_getValue:					return ExecuteMethod_getValue(p_method_id, p_arguments, p_interpreter);
		case gEidosID_identicalContents:		return ExecuteMethod_identicalContents(p_method_id, p_arguments, p_interpreter);
		case gEidosID_serialize:				return ExecuteMethod_serialize(p_method_id, p_arguments, p_interpreter);
		//case gEidosID_setValue:				return ExecuteMethod_Accelerated_setValue(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (void)addKeysAndValuesFrom(object$ source)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_addKeysAndValuesFrom(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *source_value = p_arguments[0].get();
	
	// Check that source is a subclass of EidosDictionaryUnretained.  We do this check here because we want to avoid making
	// EidosDictionaryUnretained visible in the public API; we want to pretend that there is just one class, Dictionary.
	// I'm not sure whether that's going to be right in the long term, but I want to keep my options open for now.
	EidosDictionaryUnretained *source = dynamic_cast<EidosDictionaryUnretained *>(source_value->ObjectElementAtIndex(0, nullptr));
	
	if (!source)
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_addKeysAndValuesFrom): addKeysAndValuesFrom() can only take values from a Dictionary or a subclass of Dictionary." << EidosTerminate(nullptr);
	
	AddKeysAndValuesFrom(source);
	
	ContentsChanged("addKeysAndValuesFrom()");
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)appendKeysAndValuesFrom(object source)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_appendKeysAndValuesFrom(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *source_value = p_arguments[0].get();
	int source_count = source_value->Count();
	
	// Loop through elements in source and handle them sequentially
	for (int value_index = 0; value_index < source_count; ++value_index)
	{
		EidosDictionaryUnretained *source = dynamic_cast<EidosDictionaryUnretained *>(source_value->ObjectElementAtIndex(value_index, nullptr));
		
		// Check that source is a subclass of EidosDictionaryUnretained.  We do this check here because we want to avoid making
		// EidosDictionaryUnretained visible in the public API; we want to pretend that there is just one class, Dictionary.
		// I'm not sure whether that's going to be right in the long term, but I want to keep my options open for now.
		if (!source)
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_appendKeysAndValuesFrom): appendKeysAndValuesFrom() can only take values from a Dictionary or a subclass of Dictionary." << EidosTerminate(nullptr);
		
		AppendKeysAndValuesFrom(source);
	}
	
	ContentsChanged("appendKeysAndValuesFrom()");
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)clearKeysAndValues(void)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_clearKeysAndValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	RemoveAllKeys();
	
	ContentsChanged("clearKeysAndValues()");
	
	return gStaticEidosValueVOID;
}

//	*********************	- (integer)compactIndices([logical$ preserveOrder = F])
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_compactIndices(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *preserveOrder_value = p_arguments[0].get();
	bool preserveOrder = preserveOrder_value->LogicalAtIndex(0, nullptr);
	
	if (!KeysAreIntegers())
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_compactIndices): compactIndices() can only be called on a dictionary that uses integer keys." << EidosTerminate(nullptr);
	
	if (KeyCount() == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	
	EidosDictionaryState_IntegerKeys *state_ptr = (EidosDictionaryState_IntegerKeys *)state_ptr_;
	EidosDictionaryHashTable_IntegerKeys &symbols = state_ptr->dictionary_symbols_;
	size_t key_count = symbols.size();
	EidosValue_Int_vector *integer_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->reserve(key_count);
	std::vector<EidosValue_SP> compacted_values;
	
	if (preserveOrder)
	{
		// to preserve the original order, we have to iterate through the 
		// sorted keys; that makes the algorithm O(n log n)
		const std::vector<int64_t> keys = SortedKeys_IntegerKeys();
		
		for (int64_t key : keys)
		{
			auto kv_pair = symbols.find(key);
			
			if (kv_pair == symbols.end())
				EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_compactIndices): (internal error) key not found in symbols." << EidosTerminate(nullptr);
			
			EidosValue *value = kv_pair->second.get();
			
			if (value->Count() > 0)
			{
				integer_result->push_int_no_check(key);
				compacted_values.push_back(kv_pair->second);
			}
		}
	}
	else
	{
		// if we don't need to preserve the order, we can do it in O(n)
		for (auto &iter : symbols)
		{
			int64_t key = iter.first;
			EidosValue *value = iter.second.get();
			
			if (value->Count() > 0)
			{
				integer_result->push_int_no_check(key);
				compacted_values.push_back(iter.second);
			}
		}
	}
	
	// clear our contents and then copy the compacted values back in
	symbols.clear();
	
	int64_t value_count = (int64_t)compacted_values.size();
	
	for (int64_t index = 0; index < value_count; ++index)
	{
		symbols[index] = std::move(compacted_values[index]);
		
		// KeyAddedToDictionary_IntegerKeys() is not needed; for us it is a no-op,
		// and DataFrame doesn't support integer keys
	}
	
	ContentsChanged("compactIndices()");
	
	return EidosValue_SP(integer_result);
}

//	*********************	- (object<Dictionary>$)getRowValues(li index, [logical$ drop = F])
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_getRowValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *index_value = p_arguments[0].get();
	EidosValue *drop_value = p_arguments[1].get();
	EidosValue_SP result_SP(nullptr);
	
	EidosDictionaryRetained *objectElement = new EidosDictionaryRetained();
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDictionaryRetained_Class));
	
	// With no columns, the indices don't matter, and the result is a new empty dictionary
	if (KeyCount() == 0)
	{
		// objectElement is now retained by result_SP, so we can release it
		objectElement->Release();
		
		return result_SP;
	}
	
	// Otherwise, we subset to get the result value for each key we contain
	// We go through the keys in sorted order, which probably doesn't matter since we're making a Dictionary, but it follows EidosDataFrame::ExecuteMethod_subsetRows()
	if (KeysAreStrings())
	{
		const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
		const std::vector<std::string> keys = SortedKeys_StringKeys();
		bool drop = drop_value->LogicalAtIndex(0, nullptr);
		
		for (const std::string &key : keys)
		{
			auto kv_pair = symbols->find(key);
			
			if (kv_pair == symbols->end())
				EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_getRowValues): (internal error) key not found in symbols." << EidosTerminate(nullptr);
			
			EidosValue_SP subset = SubsetEidosValue(kv_pair->second.get(), index_value, nullptr, /* p_raise_range_errors */ false);
			
			if (!drop || subset->Count())
				objectElement->SetKeyValue_StringKeys(kv_pair->first, subset);
		}
	}
	else	// KeysAreIntegers()
	{
		const EidosDictionaryHashTable_IntegerKeys *symbols = DictionarySymbols_IntegerKeys();
		const std::vector<int64_t> keys = SortedKeys_IntegerKeys();
		bool drop = drop_value->LogicalAtIndex(0, nullptr);
		
		for (int64_t key : keys)
		{
			auto kv_pair = symbols->find(key);
			
			if (kv_pair == symbols->end())
				EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_getRowValues): (internal error) key not found in symbols." << EidosTerminate(nullptr);
			
			EidosValue_SP subset = SubsetEidosValue(kv_pair->second.get(), index_value, nullptr, /* p_raise_range_errors */ false);
			
			if (!drop || subset->Count())
				objectElement->SetKeyValue_IntegerKeys(kv_pair->first, subset);
		}
	}
	
	objectElement->ContentsChanged("getRowValues()");
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}

//	*********************	- (*)getValue(is$ key)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *key_value = p_arguments[0].get();
	
	// see also EidosDictionaryUnretained::GetValueForKey()
	if (!state_ptr_)
		return gStaticEidosValueNULL;
	
	if (KeysAreStrings())
	{
		if (key_value->Type() != EidosValueType::kValueString)
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_getValue): an integer key was supplied to getValue(), but the target dictionary uses string keys." << EidosTerminate(nullptr);
		
		const std::string &key = ((EidosValue_String *)key_value)->StringRefAtIndex(0, nullptr);
		const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
		
		auto found_iter = symbols->find(key);
		
		if (found_iter == symbols->end())
			return gStaticEidosValueNULL;
		else
			return found_iter->second;
	}
	else	// KeysAreIntegers()
	{
		if (key_value->Type() != EidosValueType::kValueInt)
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_getValue): a string key was supplied to getValue(), but the target dictionary uses integer keys." << EidosTerminate(nullptr);
		
		int64_t key = ((EidosValue_Int *)key_value)->IntAtIndex(0, nullptr);
		const EidosDictionaryHashTable_IntegerKeys *symbols = DictionarySymbols_IntegerKeys();
		
		auto found_iter = symbols->find(key);
		
		if (found_iter == symbols->end())
			return gStaticEidosValueNULL;
		else
			return found_iter->second;
	}
}

//	*********************	- (logical$)identicalContents(object$ x)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_identicalContents(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *x_value = p_arguments[0].get();
	EidosObject *x_object = x_value->ObjectElementAtIndex(0, nullptr);
	EidosDictionaryUnretained *x_dict = dynamic_cast<EidosDictionaryUnretained *>(x_object);
	
	if (!x_dict)
		return gStaticEidosValue_LogicalF;
	
	if ((KeysAreStrings() && !x_dict->KeysAreStrings()) ||
		(KeysAreIntegers() && !x_dict->KeysAreIntegers()))
		return gStaticEidosValue_LogicalF;
	
	int keycount = KeyCount();
	int x_keycount = x_dict->KeyCount();
	
	if (keycount != x_keycount)
		return gStaticEidosValue_LogicalF;
	
	if (keycount == 0)
		return gStaticEidosValue_LogicalT;
	
	// At this point we know that x is a dictionary, with the same (non-zero) number of keys as us
	// For DataFrame we now ensure the columns are in the same order; for Dictionary, keys are
	// in sorted order, so this just compares to check that the keys are equal.
	if (KeysAreStrings())
	{
		const std::vector<std::string> x_keys = x_dict->SortedKeys_StringKeys();
		const std::vector<std::string> keys = SortedKeys_StringKeys();
		
		if (x_keys != keys)
			return gStaticEidosValue_LogicalF;
	}
	else	// KeysAreIntegers()
	{
		const std::vector<int64_t> x_keys = x_dict->SortedKeys_IntegerKeys();
		const std::vector<int64_t> keys = SortedKeys_IntegerKeys();
		
		if (x_keys != keys)
			return gStaticEidosValue_LogicalF;
	}
	
	// Now we know it has the same keys in the same order
	if (KeysAreStrings())
	{
		const EidosDictionaryHashTable_StringKeys *x_symbols = x_dict->DictionarySymbols_StringKeys();
		const EidosDictionaryHashTable_StringKeys *symbols = DictionarySymbols_StringKeys();
		
		for (auto const &kv_pair : *symbols)
		{
			const std::string &key = kv_pair.first;
			EidosValue *value = kv_pair.second.get();
			
			auto found_iter = x_symbols->find(key);
			
			if (found_iter == x_symbols->end())
				return gStaticEidosValue_LogicalF;
			
			EidosValue *found_value = found_iter->second.get();
			
			if (!IdenticalEidosValues(value, found_value))
				return gStaticEidosValue_LogicalF;
		}
	}
	else	// KeysAreIntegers()
	{
		const EidosDictionaryHashTable_IntegerKeys *x_symbols = x_dict->DictionarySymbols_IntegerKeys();
		const EidosDictionaryHashTable_IntegerKeys *symbols = DictionarySymbols_IntegerKeys();
		
		for (auto const &kv_pair : *symbols)
		{
			int64_t key = kv_pair.first;
			EidosValue *value = kv_pair.second.get();
			
			auto found_iter = x_symbols->find(key);
			
			if (found_iter == x_symbols->end())
				return gStaticEidosValue_LogicalF;
			
			EidosValue *found_value = found_iter->second.get();
			
			if (!IdenticalEidosValues(value, found_value))
				return gStaticEidosValue_LogicalF;
		}
	}
	
	return gStaticEidosValue_LogicalT;
}

//	*********************	- (void)setValue(is$ key, * value)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_Accelerated_setValue(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *key_value = p_arguments[0].get();
	const EidosValue_SP &value = p_arguments[1];
	
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		EidosDictionaryUnretained *element = (EidosDictionaryUnretained *)(p_elements[element_index]);
		
		// This method used to not call SetKeyValue(), in order to set the same value across multiple
		// targets.  That made me nervous, and was hard to reconcile with DataFrame, so I removed it.
		if (key_value->Type() == EidosValueType::kValueString)
		{
			const std::string &key = ((EidosValue_String *)key_value)->StringRefAtIndex(0, nullptr);
			
			element->SetKeyValue_StringKeys(key, value);
		}
		else
		{
			int64_t key = ((EidosValue_Int *)key_value)->IntAtIndex(0, nullptr);
			
			element->SetKeyValue_IntegerKeys(key, value);
		}
		
		element->ContentsChanged("setValue()");
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	- (string)serialize([string$ format = "slim"])
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_serialize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue_String *string_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &format_name = string_value->StringRefAtIndex(0, nullptr);
	
	if (format_name == "slim")
	{
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(Serialization_SLiM()));
	}
	else if (format_name == "json")
	{
		nlohmann::json json_rep = JSONRepresentation();
		std::string json_string = json_rep.dump();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(json_string));
	}
	else if (format_name == "csv")
	{
		return Serialization_CSV(",");
	}
	else if (format_name == "tsv")
	{
		return Serialization_CSV("\t");
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_serialize): serialize() does not recognize the format '" << format_name << "'; it should be 'slim', 'json', 'csv', or 'tsv'." << EidosTerminate(nullptr);
	}
}


//
//	EidosDictionaryUnretained_Class
//
#pragma mark -
#pragma mark EidosDictionaryUnretained_Class
#pragma mark -

EidosClass *gEidosDictionaryUnretained_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *EidosDictionaryUnretained_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosDictionaryUnretained_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_allKeys,				true,	kEidosValueMaskInt | kEidosValueMaskString)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosDictionaryUnretained_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosDictionaryUnretained_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_addKeysAndValuesFrom, kEidosValueMaskVOID))->AddObject_S(gEidosStr_source, nullptr));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_appendKeysAndValuesFrom, kEidosValueMaskVOID))->AddObject(gEidosStr_source, nullptr));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_clearKeysAndValues, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_compactIndices, kEidosValueMaskInt, gEidosDictionaryRetained_Class))->AddLogical_OS("preserveOrder", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_getRowValues, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class))->AddArg(kEidosValueMaskLogical | kEidosValueMaskInt, "index", nullptr)->AddLogical_OS("drop", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_getValue, kEidosValueMaskAny))->AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskSingleton, "key", nullptr));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_identicalContents, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddObject_S("x", nullptr));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_serialize, kEidosValueMaskString))->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("slim"))));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_setValue, kEidosValueMaskVOID))->AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskSingleton, "key", nullptr)->AddAny("value"))->DeclareAcceleratedImp(EidosDictionaryUnretained::ExecuteMethod_Accelerated_setValue));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}


//
// EidosDictionaryRetained
//
#pragma mark -
#pragma mark EidosDictionaryRetained
#pragma mark -

/*
EidosDictionaryRetained::EidosDictionaryRetained(void)
{
//	std::cerr << "EidosDictionaryRetained::EidosDictionaryRetained allocated " << this << " with refcount == 1" << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}

EidosDictionaryRetained::~EidosDictionaryRetained(void)
{
//	std::cerr << "EidosDictionaryRetained::~EidosDictionaryRetained deallocated " << this << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}
*/

void EidosDictionaryRetained::SelfDelete(void)
{
	// called when our refcount reaches zero; can be overridden by subclasses to provide custom behavior
	// the default behavior assumes that this was allocated by new, and uses delete to free it
	delete this;
}

void EidosDictionaryRetained::ConstructFromEidos(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter, const std::string &p_caller_name, const std::string &p_constructor_name)
{
	if (p_arguments.size() == 0)
	{
		// Create a new empty Dictionary
	}
	else if (p_arguments.size() == 1)
	{
		// one singleton argument; multiple overloaded meanings
		EidosValue *source_value = p_arguments[0].get();
		
		if (source_value->Count() != 1)
			EIDOS_TERMINATION << "ERROR (" << p_caller_name << "): " << p_constructor_name << "(x) requires that x be a singleton (Dictionary, Dictionary subclass, or JSON string)." << EidosTerminate(nullptr);
		
		if (source_value->Type() == EidosValueType::kValueString)
		{
			// Construct from a JSON string
			std::string json_string = source_value->StringAtIndex(0, nullptr);
			nlohmann::json json_rep;
			
			try {
				json_rep = nlohmann::json::parse(json_string);
			} catch (...) {
				EIDOS_TERMINATION << "ERROR (" << p_caller_name << "): the string$ argument passed to " << p_constructor_name << "() does not parse as a valid JSON string." << EidosTerminate(nullptr);
			}
			
			AddJSONFrom(json_rep);
		}
		else
		{
			// Construct from a Dictionary or Dictionary subclass
			EidosDictionaryUnretained *source = (source_value->Type() != EidosValueType::kValueObject) ? nullptr : dynamic_cast<EidosDictionaryUnretained *>(source_value->ObjectElementAtIndex(0, nullptr));
		
			if (!source)
				EIDOS_TERMINATION << "ERROR (" << p_caller_name << "): " << p_constructor_name << "(x) requires that x be a singleton Dictionary (or a singleton subclass of Dictionary)." << EidosTerminate(nullptr);
		
			AddKeysAndValuesFrom(source);
		}
	}
	else
	{
		// Set key-value pairs on the new Dictionary
		int arg_count = (int)p_arguments.size();
		
		if (arg_count % 2 != 0)
			EIDOS_TERMINATION << "ERROR (" << p_caller_name << "): " << p_constructor_name << "(...) requires an even number of arguments (comprising key-value pairs)." << EidosTerminate(nullptr);
		
		int kv_count = arg_count / 2;
		
		for (int kv_index = 0; kv_index < kv_count; ++kv_index)
		{
			EidosValue *key = p_arguments[(size_t)kv_index * 2].get();
			const EidosValue_SP &value = p_arguments[(size_t)kv_index * 2 + 1];
			
			if (key->Count() != 1)
				EIDOS_TERMINATION << "ERROR (" << p_caller_name << "): " << p_constructor_name << " requires that keys be singletons." << EidosTerminate(nullptr);
			
			if (key->Type() == EidosValueType::kValueString)
			{
				EidosValue_String *key_string_value = (EidosValue_String *)key;
				
				SetKeyValue_StringKeys(key_string_value->StringRefAtIndex(0, nullptr), value);
			}
			else if (key->Type() == EidosValueType::kValueInt)
			{
				EidosValue_Int *key_integer_value = (EidosValue_Int *)key;
				
				SetKeyValue_IntegerKeys(key_integer_value->IntAtIndex(0, nullptr), value);
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (" << p_caller_name << "): " << p_constructor_name << " requires that keys be of type string or integer." << EidosTerminate(nullptr);
			}
		}
	}
	
	// The caller must call ContentsChanged()
}

//	(object<Dictionary>$)Dictionary(...)
static EidosValue_SP Eidos_Instantiate_EidosDictionaryRetained(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosDictionaryRetained *objectElement = new EidosDictionaryRetained();
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDictionaryRetained_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	objectElement->ConstructFromEidos(p_arguments, p_interpreter, "Eidos_Instantiate_EidosDictionaryRetained", "Dictionary");
	objectElement->ContentsChanged("Dictionary()");
	
	return result_SP;
}


//
// EidosDictionaryRetained Eidos support
//
#pragma mark -
#pragma mark EidosDictionaryRetained Eidos support
#pragma mark -

const EidosClass *EidosDictionaryRetained::Class(void) const
{
	return gEidosDictionaryRetained_Class;
}


//
//	EidosDictionaryRetained_Class
//
#pragma mark -
#pragma mark EidosDictionaryRetained_Class
#pragma mark -

EidosClass *gEidosDictionaryRetained_Class = nullptr;


const std::vector<EidosFunctionSignature_CSP> *EidosDictionaryRetained_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosDictionaryRetained_Class::Functions(): not warmed up");
		
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Dictionary, Eidos_Instantiate_EidosDictionaryRetained, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class))->AddEllipsis());
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}

bool EidosDictionaryRetained_Class::UsesRetainRelease(void) const
{
	return true;
}










































