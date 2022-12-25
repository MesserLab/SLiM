//
//  eidos_functions_strings.cpp
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

#include <string>
#include <vector>
#include <regex>


// ************************************************************************************
//
//	string manipulation functions
//

#pragma mark -
#pragma mark String manipulation functions
#pragma mark -


//	(lis)grep(string$ pattern, string x, [logical$ ignoreCase = F], [string$ grammar = "ECMAScript"], [string$ value = "indices"], [logical$ fixed = F], [logical$ invert = F])
EidosValue_SP Eidos_ExecuteFunction_grep(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_String *pattern_value = (EidosValue_String *)(p_arguments[0].get());
	EidosValue_String *x_value = (EidosValue_String *)(p_arguments[1].get());
	EidosValue_Logical *ignoreCase_value = (EidosValue_Logical *)(p_arguments[2].get());
	EidosValue_String *grammar_value = (EidosValue_String *)(p_arguments[3].get());
	EidosValue_String *value_value = (EidosValue_String *)(p_arguments[4].get());
	EidosValue_Logical *fixed_value = (EidosValue_Logical *)(p_arguments[5].get());
	EidosValue_Logical *invert_value = (EidosValue_Logical *)(p_arguments[6].get());
	
	// Figure out our parameters
	const std::string &pattern = pattern_value->StringRefAtIndex(0, nullptr);
	size_t pattern_length = pattern.length();
	int x_count = x_value->Count();
	bool ignoreCase = ignoreCase_value->LogicalAtIndex(0, nullptr);
	const std::string &grammar = grammar_value->StringRefAtIndex(0, nullptr);
	const std::string &value = value_value->StringRefAtIndex(0, nullptr);
	bool fixed = fixed_value->LogicalAtIndex(0, nullptr);
	bool invert = invert_value->LogicalAtIndex(0, nullptr);
	
	if (pattern_length == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_grep): function grep() requires pattern to be of length >= 1." << EidosTerminate(nullptr);
	
	std::regex_constants::syntax_option_type grammar_enum;
	
	if (grammar == "ECMAScript")	grammar_enum = std::regex_constants::ECMAScript;
	else if (grammar == "basic")	grammar_enum = std::regex_constants::basic;
	else if (grammar == "extended")	grammar_enum = std::regex_constants::extended;
	else if (grammar == "awk")		grammar_enum = std::regex_constants::awk;
	else if (grammar == "grep")		grammar_enum = std::regex_constants::grep;
	else if (grammar == "egrep")	grammar_enum = std::regex_constants::egrep;
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_grep): function grep() requires grammar to be one of 'ECMAScript', 'basic', 'extended', 'awk', 'grep', or 'egrep'." << EidosTerminate(nullptr);
	
	enum Eidos_grepValueType {
		kIndices = 0,
		kElements,
		kMatches,
		kLogical
	};
	
	Eidos_grepValueType value_enum;
	
	if (value == "indices")			value_enum = kIndices;
	else if (value == "elements")	value_enum = kElements;
	else if (value == "matches")	value_enum = kMatches;
	else if (value == "logical")	value_enum = kLogical;
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_grep): function grep() requires value to be one of 'indices', 'elements', 'matches', or 'logical'." << EidosTerminate(nullptr);
	
	if (invert && (value_enum == kMatches))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_grep): function grep() does not allow value='matches' when invert=T." << EidosTerminate(nullptr);
	
	// Make our return value
	EidosValue_SP result_SP(nullptr);
	EidosValue_Logical *result_logical = nullptr;
	EidosValue_Int_vector *result_int = nullptr;
	EidosValue_String_vector *result_string = nullptr;
	
	if (value_enum == kIndices)
	{
		result_int = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
		result_SP = EidosValue_SP(result_int);
	}
	else if ((value_enum == kElements) || (value_enum == kMatches))
	{
		result_string = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
		result_SP = EidosValue_SP(result_string);
	}
	else if (value_enum == kLogical)
	{
		result_logical = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(result_logical);
	}
	
	// Do the matching, producing the appropriate value type into the result
	if (fixed)
	{
		// pattern is a fixed string, so use basic C++ string searching, honoring ignoreCase and invert
		for (int i = 0; i < x_count; ++i)
		{
			const std::string &x_element = x_value->StringRefAtIndex(i, nullptr);
			size_t match_pos = std::string::npos;	// not valid when invert==T, which is why "match" is disallowed then
			bool is_match = false;
			
			if (ignoreCase)
			{
				// see https://stackoverflow.com/a/19839371/2752221
				auto iter = std::search(x_element.begin(), x_element.end(),
									  pattern.begin(),   pattern.end(),
									  [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
				is_match = (iter != x_element.end());
				if (is_match)
					match_pos = iter - x_element.begin();
			}
			else
			{
				match_pos = x_element.find(pattern, 0);
				is_match = (match_pos != std::string::npos);
			}
			
			if (invert)
				is_match = !is_match;
			
			if (is_match)
			{
				if (value_enum == kIndices)
				{
					result_int->push_int(i);
				}
				else if (value_enum == kElements)
				{
					std::string x_element_copy = x_element;
					result_string->PushString(x_element_copy);
				}
				else if (value_enum == kMatches)
				{
					std::string matched_substring = x_element.substr(match_pos, pattern_length);
					result_string->PushString(matched_substring);
				}
				else if (value_enum == kLogical)
				{
					result_logical->set_logical_no_check(true, i);
				}
			}
			else
			{
				if (value_enum == kLogical)
					result_logical->set_logical_no_check(false, i);
			}
		}
	}
	else
	{
		if (!Eidos_RegexWorks())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_grep): This build of Eidos does not have a working <regex> library, due to a bug in the underlying C++ standard library provided by the system.  Calls to grep() with fixed=F, to do regular expression matching, are therefore not allowed.  This problem might be resolved by updating your compiler or toolchain, or by upgrading to a more recent version of your operating system." << EidosTerminate(nullptr);
		
		// pattern is a regular expression, so use <regex> to find matches using grammar, honoring ignoreCase and invert
		if (ignoreCase)
			grammar_enum |= std::regex_constants::icase;
		
		std::regex pattern_regex(pattern, grammar_enum);
		
		for (int i = 0; i < x_count; ++i)
		{
			const std::string &x_element = x_value->StringRefAtIndex(i, nullptr);
			std::smatch match_info;
			bool is_match = std::regex_search(x_element, match_info, pattern_regex);
			
			if (invert)
				is_match = !is_match;
			
			if (is_match)
			{
				if (value_enum == kIndices)
				{
					result_int->push_int(i);
				}
				else if (value_enum == kElements)
				{
					std::string x_element_copy = x_element;
					result_string->PushString(x_element_copy);
				}
				else if (value_enum == kMatches)
				{
					std::string matched_substring = match_info.str(0);
					result_string->PushString(matched_substring);
				}
				else if (value_enum == kLogical)
				{
					result_logical->set_logical_no_check(true, i);
				}
			}
			else
			{
				if (value_enum == kLogical)
					result_logical->set_logical_no_check(false, i);
			}
		}
	}
	
	return result_SP;
}

//	(integer)nchar(string x)
EidosValue_SP Eidos_ExecuteFunction_nchar(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *x_value = (EidosValue_String *)(p_arguments[0].get());
	int x_count = x_value->Count();
	
	if (x_count == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(x_value->StringRefAtIndex(0, nullptr).size()));
	}
	else
	{
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
			int_result->set_int_no_check(string_vec[value_index].size(), value_index);
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)strcontains(string x, string$ s, [i$ pos = 0])
EidosValue_SP Eidos_ExecuteFunction_strcontains(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *x_value = (EidosValue_String *)(p_arguments[0].get());
	EidosValue_String *s_value = (EidosValue_String *)(p_arguments[1].get());
	EidosValue_Int *pos_value = (EidosValue_Int *)(p_arguments[2].get());
	
	int x_count = x_value->Count();
	const std::string &s = s_value->StringRefAtIndex(0, nullptr);
	int64_t pos = pos_value->IntAtIndex(0, nullptr);
	
	if (s.length() == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_strcontains): function strcontains() requires s to be of length >= 1." << EidosTerminate(nullptr);
	if (pos < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_strcontains): function strcontains() requires pos to be >= 0." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		const std::string &x = x_value->StringRefAtIndex(0, nullptr);
		size_t index = x.find(s, pos);
		
		if (x_value ->DimensionCount() == 1)
			result_SP = (index != std::string::npos ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{index != std::string::npos});
	}
	else
	{
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			size_t index = string_vec[value_index].find(s, pos);
			logical_result->set_logical_no_check(index != std::string::npos, value_index);
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(integer)strfind(string x, string$ s, [i$ pos = 0])
EidosValue_SP Eidos_ExecuteFunction_strfind(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *x_value = (EidosValue_String *)(p_arguments[0].get());
	EidosValue_String *s_value = (EidosValue_String *)(p_arguments[1].get());
	EidosValue_Int *pos_value = (EidosValue_Int *)(p_arguments[2].get());
	
	int x_count = x_value->Count();
	const std::string &s = s_value->StringRefAtIndex(0, nullptr);
	int64_t pos = pos_value->IntAtIndex(0, nullptr);
	
	if (s.length() == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_strfind): function strfind() requires s to be of length >= 1." << EidosTerminate(nullptr);
	if (pos < 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_strfind): function strfind() requires pos to be >= 0." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		const std::string &x = x_value->StringRefAtIndex(0, nullptr);
		size_t index = x.find(s, pos);
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(index == std::string::npos ? -1 : (int64_t)index));
	}
	else
	{
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			size_t index = string_vec[value_index].find(s, pos);
			int_result->set_int_no_check(index == std::string::npos ? -1 : (int64_t)index, value_index);
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(logical)strprefix(string x, string$ s)
EidosValue_SP Eidos_ExecuteFunction_strprefix(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *x_value = (EidosValue_String *)(p_arguments[0].get());
	EidosValue_String *s_value = (EidosValue_String *)(p_arguments[1].get());
	
	int x_count = x_value->Count();
	const std::string &s = s_value->StringRefAtIndex(0, nullptr);
	
	if (s.length() == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_strprefix): function strprefix() requires s to be of length >= 1." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		const std::string &x = x_value->StringRefAtIndex(0, nullptr);
		bool has_prefix = Eidos_string_hasPrefix(x, s);
		
		if (x_value ->DimensionCount() == 1)
			result_SP = (has_prefix ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{has_prefix});
	}
	else
	{
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			bool has_prefix = Eidos_string_hasPrefix(string_vec[value_index], s);
			logical_result->set_logical_no_check(has_prefix, value_index);
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(string)strsplit(string$ x, [string$ sep = " "])
EidosValue_SP Eidos_ExecuteFunction_strsplit(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *x_value = (EidosValue_String *)p_arguments[0].get();
	EidosValue_String *sep_value = (EidosValue_String *)p_arguments[1].get();
	EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
	result_SP = EidosValue_SP(string_result);
	
	const std::string &joined_string = x_value->StringRefAtIndex(0, nullptr);
	const std::string &separator = sep_value->StringAtIndex(0, nullptr);
	std::string::size_type start_idx = 0, sep_idx;
	
	if (separator.length() == 0)
	{
		// special-case a zero-length separator
		for (const char &ch : joined_string)
			string_result->PushString(std::string(&ch, 1));
	}
	else
	{
		// non-zero-length separator
		while (true)
		{
			sep_idx = joined_string.find(separator, start_idx);
			
			if (sep_idx == std::string::npos)
			{
				string_result->PushString(joined_string.substr(start_idx));
				break;
			}
			else
			{
				string_result->PushString(joined_string.substr(start_idx, sep_idx - start_idx));
				start_idx = sep_idx + separator.size();
			}
		}
	}
	
	return result_SP;
}

//	(logical)strsuffix(string x, string$ s)
EidosValue_SP Eidos_ExecuteFunction_strsuffix(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *x_value = (EidosValue_String *)(p_arguments[0].get());
	EidosValue_String *s_value = (EidosValue_String *)(p_arguments[1].get());
	
	int x_count = x_value->Count();
	const std::string &s = s_value->StringRefAtIndex(0, nullptr);
	
	if (s.length() == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_strsuffix): function strsuffix() requires s to be of length >= 1." << EidosTerminate(nullptr);
	
	if (x_count == 1)
	{
		const std::string &x = x_value->StringRefAtIndex(0, nullptr);
		bool has_prefix = Eidos_string_hasSuffix(x, s);
		
		if (x_value ->DimensionCount() == 1)
			result_SP = (has_prefix ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		else
			result_SP = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{has_prefix});
	}
	else
	{
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(x_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			bool has_prefix = Eidos_string_hasSuffix(string_vec[value_index], s);
			logical_result->set_logical_no_check(has_prefix, value_index);
		}
	}
	
	result_SP->CopyDimensionsFromValue(x_value);
	
	return result_SP;
}

//	(string)substr(string x, integer first, [Ni last = NULL])
EidosValue_SP Eidos_ExecuteFunction_substr(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *x_value = (EidosValue_String *)p_arguments[0].get();
	int x_count = x_value->Count();
	EidosValue *arg_last = p_arguments[2].get();
	EidosValueType arg_last_type = arg_last->Type();
	
	if (x_count == 1)
	{
		const std::string &string_value = x_value->StringRefAtIndex(0, nullptr);
		int64_t len = (int64_t)string_value.size();
		EidosValue *arg_first = p_arguments[1].get();
		int arg_first_count = arg_first->Count();
		
		if (arg_first_count != x_count)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of first to be 1, or equal to the size of x." << EidosTerminate(nullptr);
		
		int64_t first0 = arg_first->IntAtIndex(0, nullptr);
		
		if (arg_last_type != EidosValueType::kValueNULL)
		{
			// last supplied
			int arg_last_count = arg_last->Count();
			
			if (arg_last_count != x_count)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of last to be 1, or equal to the size of x." << EidosTerminate(nullptr);
			
			int64_t last0 = arg_last->IntAtIndex(0, nullptr);
			
			int64_t clamped_first = (int)first0;
			int64_t clamped_last = (int)last0;
			
			if (clamped_first < 0) clamped_first = 0;
			if (clamped_last >= len) clamped_last = (int)len - 1;
			
			if ((clamped_first >= len) || (clamped_last < 0) || (clamped_first > clamped_last))
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_empty_string));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string_value.substr(clamped_first, clamped_last - clamped_first + 1)));
		}
		else
		{
			// last not supplied; take substrings to the end of each string
			int clamped_first = (int)first0;
			
			if (clamped_first < 0) clamped_first = 0;
			
			if (clamped_first >= len)						
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_empty_string));
			else
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(string_value.substr(clamped_first, len)));
		}
	}
	else
	{
		const std::vector<std::string> &string_vec = *x_value->StringVector();
		EidosValue *arg_first = p_arguments[1].get();
		int arg_first_count = arg_first->Count();
		bool first_singleton = (arg_first_count == 1);
		
		if (!first_singleton && (arg_first_count != x_count))
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of first to be 1, or equal to the size of x." << EidosTerminate(nullptr);
		
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(x_count);
		result_SP = EidosValue_SP(string_result);
		
		int64_t first0 = arg_first->IntAtIndex(0, nullptr);
		
		if (arg_last_type != EidosValueType::kValueNULL)
		{
			// last supplied
			int arg_last_count = arg_last->Count();
			bool last_singleton = (arg_last_count == 1);
			
			if (!last_singleton && (arg_last_count != x_count))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_substr): function substr() requires the size of last to be 1, or equal to the size of x." << EidosTerminate(nullptr);
			
			int64_t last0 = arg_last->IntAtIndex(0, nullptr);
			
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				const std::string &str = string_vec[value_index];
				int64_t len = (int64_t)str.size();
				int64_t clamped_first = (first_singleton ? first0 : arg_first->IntAtIndex(value_index, nullptr));
				int64_t clamped_last = (last_singleton ? last0 : arg_last->IntAtIndex(value_index, nullptr));
				
				if (clamped_first < 0) clamped_first = 0;
				if (clamped_last >= len) clamped_last = (int)len - 1;
				
				if ((clamped_first >= len) || (clamped_last < 0) || (clamped_first > clamped_last))
					string_result->PushString(gEidosStr_empty_string);
				else
					string_result->PushString(str.substr(clamped_first, clamped_last - clamped_first + 1));
			}
		}
		else
		{
			// last not supplied; take substrings to the end of each string
			for (int value_index = 0; value_index < x_count; ++value_index)
			{
				const std::string &str = string_vec[value_index];
				int64_t len = (int64_t)str.size();
				int64_t clamped_first = (first_singleton ? first0 : arg_first->IntAtIndex(value_index, nullptr));
				
				if (clamped_first < 0) clamped_first = 0;
				
				if (clamped_first >= len)						
					string_result->PushString(gEidosStr_empty_string);
				else
					string_result->PushString(str.substr(clamped_first, len));
			}
		}
	}
	
	return result_SP;
}
















































