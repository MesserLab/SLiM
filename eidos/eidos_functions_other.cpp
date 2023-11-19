//
//  eidos_functions_other.cpp
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
#include "eidos_call_signature.h"
#include "eidos_rng.h"
#include "eidos_beep.h"
#include "eidos_openmp.h"

#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>
#include <fstream>
#include <sys/utsname.h>
#include <chrono>
#include <ctime>
#include <algorithm>

#if 0
// These would enable further keys in sysinfo(), but cause problems on Ubuntu 18.04 and/or Windows
#include <pwd.h>
#include <uuid/uuid.h>
#endif


// ************************************************************************************
//
//	miscellaneous functions
//
#pragma mark -
#pragma mark Miscellaneous functions
#pragma mark -


//	(void)assert(logical assertions, [Ns$ message = NULL])
EidosValue_SP Eidos_ExecuteFunction_assert(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue *assertions_value = p_arguments[0].get();
	
	// determine whether the assertions vector is all true
	int assertions_count = assertions_value->Count();
	const eidos_logical_t *logical_data = assertions_value->LogicalVector()->data();
	bool any_false = false;
	
	for (int assertions_index = 0; assertions_index < assertions_count; ++assertions_index)
		if (!logical_data[assertions_index])
		{
			any_false = true;
			break;
		}
	
	// stop with a message if an assertion is false
	if (any_false)
	{
		EidosValue *message_value = p_arguments[1].get();
		
		if (message_value->Type() != EidosValueType::kValueNULL)
		{
			std::string &&stop_string = message_value->StringAtIndex(0, nullptr);
			
			p_interpreter.ErrorOutputStream() << stop_string << std::endl;
			
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_assert): assertion failed: " << stop_string << "." << EidosTerminate(nullptr);
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_assert): assertion failed." << EidosTerminate(nullptr);
		}
	}
	
	return gStaticEidosValueVOID;
}

//	(void)beep([Ns$ soundName = NULL])
EidosValue_SP Eidos_ExecuteFunction_beep(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Eidos_ExecuteFunction_beep(): main thread only");
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *soundName_value = p_arguments[0].get();
	std::string name_string = ((soundName_value->Type() == EidosValueType::kValueString) ? soundName_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	
	std::string beep_error = Eidos_Beep(name_string);
	
	if (beep_error.length())
	{
		if (!gEidosSuppressWarnings)
		{
			std::ostream &output_stream = p_interpreter.ErrorOutputStream();
		
			output_stream << beep_error << std::endl;
		}
	}
	
	return gStaticEidosValueVOID;
}

//	(void)citation(void)
EidosValue_SP Eidos_ExecuteFunction_citation(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "To cite Eidos in publications please use:" << std::endl << std::endl;
	output_stream << "Haller, B.C. (2016). Eidos: A Simple Scripting Language." << std::endl;
	output_stream << "URL: http://benhaller.com/slim/Eidos_Manual.pdf" << std::endl << std::endl;
	
	if (gEidosContextCitation.length())
	{
		output_stream << "---------------------------------------------------------" << std::endl << std::endl;
		output_stream << gEidosContextCitation << std::endl;
	}
	
	return gStaticEidosValueVOID;
}

//	(float$)clock([string$ type = "cpu"])
static std::chrono::steady_clock::time_point timebase = std::chrono::steady_clock::now();	// start at launch

EidosValue_SP Eidos_ExecuteFunction_clock(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_String *string_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &type_name = string_value->StringRefAtIndex(0, nullptr);
	
	if (type_name == "cpu")
	{
		// elapsed CPU time; this is across all cores, so it can be larger than the elapsed wall clock time!
		std::clock_t cpu_time = std::clock();
		double cpu_time_d = static_cast<double>(cpu_time) / CLOCKS_PER_SEC;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(cpu_time_d));
	}
	else if (type_name == "mono")
	{
		// monotonic clock time; this is best for measured user-perceived elapsed times
		std::chrono::steady_clock::time_point ts = std::chrono::steady_clock::now();
		std::chrono::steady_clock::duration clock_duration = ts - timebase;
		double seconds = std::chrono::duration<double>(clock_duration).count();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(seconds));
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_clock): unrecognized clock type " << type_name << " in function clock()." << EidosTerminate(nullptr);
	}
}

//	(string$)date(void)
EidosValue_SP Eidos_ExecuteFunction_date(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	time_t rawtime;
	struct tm timeinfo;
	char buffer[25];	// should never be more than 10, in fact, plus a null
	
	time(&rawtime);
	localtime_r(&rawtime, &timeinfo);
	strftime(buffer, 25, "%d-%m-%Y", &timeinfo);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(std::string(buffer)));
	
	return result_SP;
}

//	(string$)debugIndent(void)
EidosValue_SP Eidos_ExecuteFunction_debugIndent(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
#if DEBUG_POINTS_ENABLED
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(EidosDebugPointIndent::Indent()));
#else
	return gStaticEidosValue_StringEmpty;
#endif
}

//	(void)defineConstant(string$ symbol, * x)
EidosValue_SP Eidos_ExecuteFunction_defineConstant(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_String *symbol_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &symbol_name = symbol_value->StringRefAtIndex(0, nullptr);
	const EidosValue_SP &x_value_sp = p_arguments[1];
	EidosGlobalStringID symbol_id = EidosStringRegistry::GlobalStringIDForString(symbol_name);
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	// Object values can only be remembered if their class is under retain/release, so that we have control over the object lifetime
	// See also EidosDictionaryUnretained::ExecuteMethod_Accelerated_setValue() and Eidos_ExecuteFunction_defineGlobal(), which enforce the same rule
	if (x_value_sp->Type() == EidosValueType::kValueObject)
	{
		const EidosClass *x_value_class = ((EidosValue_Object *)x_value_sp.get())->Class();
		
		if (!x_value_class->UsesRetainRelease())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_defineConstant): defineConstant() can only accept object classes that are under retain/release memory management internally; class " << x_value_class->ClassName() << " is not.  This restriction is necessary in order to guarantee that the kept object elements remain valid." << EidosTerminate(nullptr);
	}
	
	symbols.DefineConstantForSymbol(symbol_id, x_value_sp);
	
	return gStaticEidosValueVOID;
}

//	(void)defineGlobal(string$ symbol, * x)
EidosValue_SP Eidos_ExecuteFunction_defineGlobal(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_String *symbol_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &symbol_name = symbol_value->StringRefAtIndex(0, nullptr);
	const EidosValue_SP &x_value_sp = p_arguments[1];
	EidosGlobalStringID symbol_id = EidosStringRegistry::GlobalStringIDForString(symbol_name);
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	// Object values can only be remembered if their class is under retain/release, so that we have control over the object lifetime
	// See also EidosDictionaryUnretained::ExecuteMethod_Accelerated_setValue() and Eidos_ExecuteFunction_defineConstant(), which enforce the same rule
	if (x_value_sp->Type() == EidosValueType::kValueObject)
	{
		const EidosClass *x_value_class = ((EidosValue_Object *)x_value_sp.get())->Class();
		
		if (!x_value_class->UsesRetainRelease())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_defineGlobal): defineGlobal() can only accept object classes that are under retain/release memory management internally; class " << x_value_class->ClassName() << " is not.  This restriction is necessary in order to guarantee that the kept object elements remain valid." << EidosTerminate(nullptr);
	}
	
	symbols.DefineGlobalForSymbol(symbol_id, x_value_sp);
	
	return gStaticEidosValueVOID;
}

//	(*)doCall(string$ functionName, ...)
EidosValue_SP Eidos_ExecuteFunction_doCall(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	int argument_count = (int)p_arguments.size();
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *functionName_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &function_name = functionName_value->StringRefAtIndex(0, nullptr);
	
	// Copy the argument list; this is a little slow, but not a big deal, and it provides protection against re-entrancy
	std::vector<EidosValue_SP> arguments;
	
	for (int argument_index = 1; argument_index < argument_count; argument_index++)
		arguments.emplace_back(p_arguments[argument_index]);
	
	// Look up the signature for this function dynamically
	EidosFunctionMap &function_map = p_interpreter.FunctionMap();
	auto signature_iter = function_map.find(function_name);
	
	if (signature_iter == function_map.end())
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): unrecognized function name " << function_name << " in function doCall().";
		if (p_interpreter.Context() == nullptr)
			EIDOS_TERMINATION << "  This may be because the current Eidos context (such as the current SLiM simulation) is invalid.";
		EIDOS_TERMINATION << EidosTerminate(nullptr);
	}
	
	const EidosFunctionSignature *function_signature = signature_iter->second.get();
	
	// Check the function's arguments
	function_signature->CheckArguments(arguments);
	
	// BEWARE!  Since the function called here could be a function, like executeLambda() or apply() or sapply(),
	// that causes re-entrancy into the Eidos engine, this call is rather dangerous.  See the comments on those
	// functions for further details.
	if (function_signature->internal_function_)
	{
		result_SP = function_signature->internal_function_(arguments, p_interpreter);
	}
	else if (function_signature->body_script_)
	{
		result_SP = p_interpreter.DispatchUserDefinedFunction(*function_signature, arguments);
	}
	else if (!function_signature->delegate_name_.empty())
	{
		EidosContext *context = p_interpreter.Context();
		
		if (context)
			result_SP = context->ContextDefinedFunctionDispatch(function_name, arguments, p_interpreter);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): (internal error) function " << function_name << " is defined by the Context, but the Context is not defined." << EidosTerminate(nullptr);
	}
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_doCall): (internal error) unbound function " << function_name << "." << EidosTerminate(nullptr);
	
	// Check the return value against the signature
	function_signature->CheckReturn(*result_SP);
	
	return result_SP;
}

// This is an internal utility method that implements executeLambda() and _executeLambda_OUTER().
// The purpose of the p_execute_in_outer_scope flag is specifically for the source() function.  Since
// source() is a user-defined function, it executes in a new scope with its own variables table.
// However, we don't want that; we want it to execute the source file in the caller's scope.  So it
// uses a special internal version of executeLambda(), named _executeLambda_OUTER(), that achieves
// that by passing true here for p_execute_in_outer_scope.  Yes, this is a hack; an end-user could
// not implement source() themselves as a user-defined function without using private API.  That's
// OK, though; many Eidos built-in functions could not be implemented in Eidos themselves.
EidosValue_SP Eidos_ExecuteLambdaInternal(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter, bool p_execute_in_outer_scope)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *lambdaSource_value = p_arguments[0].get();
	EidosValue_String_singleton *lambdaSource_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[0].get());
	EidosScript *script = (lambdaSource_value_singleton ? lambdaSource_value_singleton->CachedScript() : nullptr);
	
	// Errors in lambdas should be reported for the lambda script, not for the calling script,
	// if possible.  In the GUI this does not work well, however; there, errors should be
	// reported as occurring in the call to executeLambda().  Here we save off the current
	// error context and set up the error context for reporting errors inside the lambda,
	// in case that is possible; see how exceptions are handled below.
	EidosErrorContext error_context_save = gEidosErrorContext;
	
	// We try to do tokenization and parsing once per script, by caching the script inside the EidosValue_String_singleton instance
	if (!script)
	{
		script = new EidosScript(lambdaSource_value->StringAtIndex(0, nullptr), -1);
		
		gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script, true};
		
		try
		{
			script->Tokenize();
			script->ParseInterpreterBlockToAST(true);
			
			// Note that true is passed to ParseInterpreterBlockToAST(), indicating that the script is to parse the code for the lambda
			// as if it were a top-level interpreter block, allowing functions to be defined.  We don't actually know, here, whether we
			// have been called at the top level or not, but we allow function definitions regardless.  Eidos doesn't disallow function
			// definitions inside blocks for any particularly strong reason; the reason is mostly just that such declarations look like
			// they should be scoped, because in most languages they are.  Since in Eidos they wouldn't be (no scope), we disallow them
			// to prevent confusion.  But when a function is declared inside executeLambda(), the user is presumably advanced and knows
			// what they're doing; there's no need to get in their way.  Sometimes it might be convenient to declare a function in this
			// way, particularly inside a file executed by source(); so let's  let the user do that, rather than bending over backwards
			// to prevent it (which is actually difficult, if you want to allow it when executeLambda() is called at the top level).
		}
		catch (...)
		{
			if (gEidosTerminateThrows)
				gEidosErrorContext = error_context_save;
			
			delete script;
			
			throw;
		}
		
		if (lambdaSource_value_singleton)
			lambdaSource_value_singleton->SetCachedScript(script);
	}
	
	// Execute inside try/catch so we can handle errors well
	EidosValue_String *timed_value = (EidosValue_String *)p_arguments[1].get();
	EidosValueType timed_value_type = timed_value->Type();
	bool timed = false;
	int timer_type = 0;		// cpu by default, for legacy reasons
	
	if (timed_value_type == EidosValueType::kValueLogical)
	{
		if (timed_value->LogicalAtIndex(0, nullptr))
			timed = true;
	}
	else if (timed_value_type == EidosValueType::kValueString)
	{
		const std::string &timed_string = timed_value->StringRefAtIndex(0, nullptr);
		
		if (timed_string == "cpu")
		{
			timed = true;
			timer_type = 0;		// cpu timer
		}
		else if (timed_string == "mono")
		{
			timed = true;
			timer_type = 1;		// monotonic timer
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteLambdaInternal): unrecognized clock type " << timed_string << " in function executeLambda()." << EidosTerminate(nullptr);
		}
	}
	
	std::clock_t begin_clock = 0, end_clock = 0;
	std::chrono::steady_clock::time_point begin_ts, end_ts;
	
	gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script, true};
	
	try
	{
		EidosSymbolTable *symbols = &p_interpreter.SymbolTable();									// use our own symbol table
		
		if (p_execute_in_outer_scope)
			symbols = symbols->ParentSymbolTable();
		
		EidosInterpreter interpreter(*script, *symbols, p_interpreter.FunctionMap(), p_interpreter.Context(), p_interpreter.ExecutionOutputStream(), p_interpreter.ErrorOutputStream());
		
		if (timed)
		{
			if (timer_type == 0)
				begin_clock = std::clock();
			else
				begin_ts = std::chrono::steady_clock::now();
		}
		
		// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
		// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
		// bugs that would otherwise not manifest.
		result_SP = interpreter.EvaluateInterpreterBlock(false, true);		// do not print output, return the last statement value
		
		if (timed)
		{
			if (timer_type == 0)
				end_clock = std::clock();
			else
				end_ts = std::chrono::steady_clock::now();
		}
	}
	catch (...)
	{
		// If exceptions throw, then we want to set up the error information to highlight the
		// executeLambda() that failed, since we can't highlight the actual error.  (If exceptions
		// don't throw, this catch block will never be hit; exit() will already have been called
		// and the error will have been reported from the context of the lambda script string.)
		if (gEidosTerminateThrows)
			gEidosErrorContext = error_context_save;
		
		if (!lambdaSource_value_singleton)
			delete script;
		
		throw;
	}
	
	// Restore the normal error context in the event that no exception occurring within the lambda
	gEidosErrorContext = error_context_save;
	
	if (timed)
	{
		double time_spent;
		
		if (timer_type == 0)
			time_spent = static_cast<double>(end_clock - begin_clock) / CLOCKS_PER_SEC;
		else
			time_spent = std::chrono::duration<double>(end_ts - begin_ts).count();
		
		p_interpreter.ExecutionOutputStream() << "// ********** executeLambda() elapsed time: " << time_spent << std::endl;
	}
	
	if (!lambdaSource_value_singleton)
		delete script;
	
	return result_SP;
}

//	(*)executeLambda(string$ lambdaSource, [ls$ timed = F])
EidosValue_SP Eidos_ExecuteFunction_executeLambda(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	return Eidos_ExecuteLambdaInternal(p_arguments, p_interpreter, false);
}

//	(*)_executeLambda_OUTER(string$ lambdaSource, [ls$ timed = F])
EidosValue_SP Eidos_ExecuteFunction__executeLambda_OUTER(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	return Eidos_ExecuteLambdaInternal(p_arguments, p_interpreter, true);	// see Eidos_ExecuteLambdaInternal() for comments on the true flag
}

//	(logical)exists(string symbol)
EidosValue_SP Eidos_ExecuteFunction_exists(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	EidosValue_String *symbol_value = (EidosValue_String *)p_arguments[0].get();
	int symbol_count = symbol_value->Count();
	
	if ((symbol_count == 1) && (symbol_value->DimensionCount() == 1))
	{
		// Use the global constants, but only if we do not have to impose a dimensionality upon the value below
		EidosGlobalStringID symbol_id = EidosStringRegistry::GlobalStringIDForString(symbol_value->StringRefAtIndex(0, nullptr));
		
		result_SP = (symbols.ContainsSymbol(symbol_id) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(symbol_count);
		result_SP = EidosValue_SP(logical_result);
		
		for (int value_index = 0; value_index < symbol_count; ++value_index)
		{
			EidosGlobalStringID symbol_id = EidosStringRegistry::GlobalStringIDForString(symbol_value->StringRefAtIndex(value_index, nullptr));
			
			logical_result->set_logical_no_check(symbols.ContainsSymbol(symbol_id), value_index);
		}
		
		result_SP->CopyDimensionsFromValue(symbol_value);
	}
	
	return result_SP;
}

//	(void)functionSignature([Ns$ functionName = NULL])
EidosValue_SP Eidos_ExecuteFunction_functionSignature(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *functionName_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	bool function_name_specified = (functionName_value->Type() == EidosValueType::kValueString);
	std::string match_string = (function_name_specified ? functionName_value->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
	bool signature_found = false;
	
	// function_map_ is already alphabetized since maps keep sorted order
	EidosFunctionMap &function_map = p_interpreter.FunctionMap();
	
	for (const auto &functionPairIter : function_map)
	{
		const EidosFunctionSignature *iter_signature = functionPairIter.second.get();
		
		if (function_name_specified && (iter_signature->call_name_.compare(match_string) != 0))
			continue;
		
		if (!function_name_specified && (iter_signature->call_name_.substr(0, 1).compare("_") == 0))
			continue;	// skip internal functions that start with an underscore, unless specifically requested
		
		output_stream << *iter_signature;
		
		if (iter_signature->body_script_ && iter_signature->user_defined_)
		{
			output_stream << " <user-defined>";
		}
		
		output_stream << std::endl;
		
		signature_found = true;
	}
	
	if (function_name_specified && !signature_found)
	{
		output_stream << "No function signature found for '" << match_string << "'.";
		if (p_interpreter.Context() == nullptr)
			output_stream << "  This may be because the current Eidos context (such as the current SLiM simulation) is invalid.";
		output_stream << std::endl;
	}
	
	return gStaticEidosValueVOID;
}

//	(void)functionSource(s$ functionName)
EidosValue_SP Eidos_ExecuteFunction_functionSource(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *functionName_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	std::string match_string = functionName_value->StringAtIndex(0, nullptr);
	
	// function_map_ is already alphabetized since maps keep sorted order
	EidosFunctionMap &function_map = p_interpreter.FunctionMap();
	
	for (const auto &functionPairIter : function_map)
	{
		const EidosFunctionSignature *iter_signature = functionPairIter.second.get();
		
		if (iter_signature->call_name_.compare(match_string) != 0)
			continue;
		
		output_stream << *iter_signature;
		
		if (iter_signature->body_script_ && iter_signature->user_defined_)
		{
			output_stream << " <user-defined>";
		}
		
		output_stream << std::endl;
		
		if (iter_signature->body_script_)
			output_stream << iter_signature->body_script_->String() << std::endl;
		else
			output_stream << "no Eidos source available (implemented in C++)" << std::endl;
		
		return gStaticEidosValueVOID;
	}
	
	output_stream << "No function found for '" << match_string << "'.";
	if (p_interpreter.Context() == nullptr)
		output_stream << "  This may be because the current Eidos context (such as the current SLiM simulation) is invalid.";
	output_stream << std::endl;
	
	return gStaticEidosValueVOID;
}

//	(integer$)getSeed(void)
EidosValue_SP Eidos_ExecuteFunction_getSeed(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	unsigned long int last_seed;
	
#pragma omp critical (Eidos_RNG_State)
	{
		Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(0);	// thread 0 has the original RNG seed requested by the user
		
		last_seed = rng_state->rng_last_seed_;
	}
	
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	EidosValue_SP result_SP(nullptr);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(last_seed));
	
	return result_SP;
}

//	(void)license(void)
EidosValue_SP Eidos_ExecuteFunction_license(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "Eidos is free software: you can redistribute it and/or" << std::endl;
	output_stream << "modify it under the terms of the GNU General Public" << std::endl;
	output_stream << "License as published by the Free Software Foundation," << std::endl;
	output_stream << "either version 3 of the License, or (at your option)" << std::endl;
	output_stream << "any later version." << std::endl << std::endl;
	
	output_stream << "Eidos is distributed in the hope that it will be" << std::endl;
	output_stream << "useful, but WITHOUT ANY WARRANTY; without even the" << std::endl;
	output_stream << "implied warranty of MERCHANTABILITY or FITNESS FOR" << std::endl;
	output_stream << "A PARTICULAR PURPOSE.  See the GNU General Public" << std::endl;
	output_stream << "License for more details." << std::endl << std::endl;
	
	output_stream << "You should have received a copy of the GNU General" << std::endl;
	output_stream << "Public License along with Eidos.  If not, see" << std::endl;
	output_stream << "<http://www.gnu.org/licenses/>." << std::endl << std::endl;
	
	if (gEidosContextLicense.length())
	{
		output_stream << "---------------------------------------------------------" << std::endl << std::endl;
		output_stream << gEidosContextLicense << std::endl;
	}
	
	return gStaticEidosValueVOID;
}

//	(void)ls([logical$ showSymbolTables = F])
EidosValue_SP Eidos_ExecuteFunction_ls(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	bool showSymbolTables = p_arguments[0]->LogicalAtIndex(0, nullptr);
	
	std::ostream &outstream = p_interpreter.ExecutionOutputStream();
	EidosSymbolTable &current_symbol_table = p_interpreter.SymbolTable();
	
	if (showSymbolTables)
	{
		EidosSymbolTable *table = &current_symbol_table;
		
		while (table)
		{
			table->PrintSymbolTable(outstream);
			outstream << std::endl;
			
			// Go to the next symbol table up in the chain; note that we use ChainSymbolTable(), not ParentSymbolTable(),
			// because we only want to show symbol tables that are relevant to the current scope
			table = table->ChainSymbolTable();
		}
	}
	else
	{
		outstream << current_symbol_table;
	}
	
	return gStaticEidosValueVOID;
}

//	(integer$)parallelGetNumThreads(void)
EidosValue_SP Eidos_ExecuteFunction_parallelGetNumThreads(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidosNumThreads));
	
	return result_SP;
}

//	(integer$)parallelGetMaxThreads(void)
EidosValue_SP Eidos_ExecuteFunction_parallelGetMaxThreads(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidosMaxThreads));
	
	return result_SP;
}

//	(object<Dictionary>$)parallelGetTaskThreadCounts(void)
EidosValue_SP Eidos_ExecuteFunction_parallelGetTaskThreadCounts(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosDictionaryRetained *objectElement = new EidosDictionaryRetained();
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDictionaryRetained_Class));
	
#ifdef _OPENMP
	objectElement->SetKeyValue_StringKeys("ABS_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_ABS_FLOAT)));
	objectElement->SetKeyValue_StringKeys("CEIL", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_CEIL)));
	objectElement->SetKeyValue_StringKeys("EXP_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_EXP_FLOAT)));
	objectElement->SetKeyValue_StringKeys("FLOOR", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_FLOOR)));
	objectElement->SetKeyValue_StringKeys("LOG_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_LOG_FLOAT)));
	objectElement->SetKeyValue_StringKeys("LOG10_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_LOG10_FLOAT)));
	objectElement->SetKeyValue_StringKeys("LOG2_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_LOG2_FLOAT)));
	objectElement->SetKeyValue_StringKeys("ROUND", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_ROUND)));
	objectElement->SetKeyValue_StringKeys("SQRT_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SQRT_FLOAT)));
	objectElement->SetKeyValue_StringKeys("SUM_INTEGER", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SUM_INTEGER)));
	objectElement->SetKeyValue_StringKeys("SUM_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SUM_FLOAT)));
	objectElement->SetKeyValue_StringKeys("SUM_LOGICAL", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SUM_LOGICAL)));
	objectElement->SetKeyValue_StringKeys("TRUNC", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_TRUNC)));
	
	objectElement->SetKeyValue_StringKeys("MAX_INT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MAX_INT)));
	objectElement->SetKeyValue_StringKeys("MAX_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MAX_FLOAT)));
	objectElement->SetKeyValue_StringKeys("MIN_INT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MIN_INT)));
	objectElement->SetKeyValue_StringKeys("MIN_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MIN_FLOAT)));
	objectElement->SetKeyValue_StringKeys("PMAX_INT_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMAX_INT_1)));
	objectElement->SetKeyValue_StringKeys("PMAX_INT_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMAX_INT_2)));
	objectElement->SetKeyValue_StringKeys("PMAX_FLOAT_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMAX_FLOAT_1)));
	objectElement->SetKeyValue_StringKeys("PMAX_FLOAT_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMAX_FLOAT_2)));
	objectElement->SetKeyValue_StringKeys("PMIN_INT_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMIN_INT_1)));
	objectElement->SetKeyValue_StringKeys("PMIN_INT_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMIN_INT_2)));
	objectElement->SetKeyValue_StringKeys("PMIN_FLOAT_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMIN_FLOAT_1)));
	objectElement->SetKeyValue_StringKeys("PMIN_FLOAT_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PMIN_FLOAT_2)));
	
	objectElement->SetKeyValue_StringKeys("MATCH_INT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MATCH_INT)));
	objectElement->SetKeyValue_StringKeys("MATCH_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MATCH_FLOAT)));
	objectElement->SetKeyValue_StringKeys("MATCH_STRING", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MATCH_STRING)));
	objectElement->SetKeyValue_StringKeys("MATCH_OBJECT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MATCH_OBJECT)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_INDEX", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_INDEX)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_R_INT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_R_INT)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_R_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_R_FLOAT)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_R_OBJECT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_R_OBJECT)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_WR_INT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_WR_INT)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_WR_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_WR_FLOAT)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_WR_OBJECT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_WR_OBJECT)));
	objectElement->SetKeyValue_StringKeys("TABULATE_MAXBIN", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_TABULATE_MAXBIN)));
	objectElement->SetKeyValue_StringKeys("TABULATE", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_TABULATE)));
	
	objectElement->SetKeyValue_StringKeys("CONTAINS_MARKER_MUT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_CONTAINS_MARKER_MUT)));
	objectElement->SetKeyValue_StringKeys("I_COUNT_OF_MUTS_OF_TYPE", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE)));
	objectElement->SetKeyValue_StringKeys("G_COUNT_OF_MUTS_OF_TYPE", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE)));
	objectElement->SetKeyValue_StringKeys("INDS_W_PEDIGREE_IDS", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_INDS_W_PEDIGREE_IDS)));
	objectElement->SetKeyValue_StringKeys("RELATEDNESS", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RELATEDNESS)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_INDIVIDUALS_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1)));
	objectElement->SetKeyValue_StringKeys("SAMPLE_INDIVIDUALS_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2)));
	objectElement->SetKeyValue_StringKeys("SET_FITNESS_SCALE_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_FITNESS_SCALE_1)));
	objectElement->SetKeyValue_StringKeys("SET_FITNESS_SCALE_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_FITNESS_SCALE_2)));
	objectElement->SetKeyValue_StringKeys("SUM_OF_MUTS_OF_TYPE", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE)));
	
	objectElement->SetKeyValue_StringKeys("DNORM_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_DNORM_1)));
	objectElement->SetKeyValue_StringKeys("DNORM_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_DNORM_2)));
	objectElement->SetKeyValue_StringKeys("RBINOM_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RBINOM_1)));
	objectElement->SetKeyValue_StringKeys("RBINOM_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RBINOM_2)));
	objectElement->SetKeyValue_StringKeys("RBINOM_3", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RBINOM_3)));
	objectElement->SetKeyValue_StringKeys("RDUNIF_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RDUNIF_1)));
	objectElement->SetKeyValue_StringKeys("RDUNIF_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RDUNIF_2)));
	objectElement->SetKeyValue_StringKeys("RDUNIF_3", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RDUNIF_3)));
	objectElement->SetKeyValue_StringKeys("REXP_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_REXP_1)));
	objectElement->SetKeyValue_StringKeys("REXP_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_REXP_2)));
	objectElement->SetKeyValue_StringKeys("RNORM_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RNORM_1)));
	objectElement->SetKeyValue_StringKeys("RNORM_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RNORM_2)));
	objectElement->SetKeyValue_StringKeys("RNORM_3", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RNORM_3)));
	objectElement->SetKeyValue_StringKeys("RPOIS_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RPOIS_1)));
	objectElement->SetKeyValue_StringKeys("RPOIS_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RPOIS_2)));
	objectElement->SetKeyValue_StringKeys("RUNIF_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RUNIF_1)));
	objectElement->SetKeyValue_StringKeys("RUNIF_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RUNIF_2)));
	objectElement->SetKeyValue_StringKeys("RUNIF_3", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_RUNIF_3)));
	
	objectElement->SetKeyValue_StringKeys("SORT_INT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SORT_INT)));
	objectElement->SetKeyValue_StringKeys("SORT_FLOAT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SORT_FLOAT)));
	objectElement->SetKeyValue_StringKeys("SORT_STRING", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SORT_STRING)));
	
	objectElement->SetKeyValue_StringKeys("POINT_IN_BOUNDS_1D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_IN_BOUNDS_1D)));
	objectElement->SetKeyValue_StringKeys("POINT_IN_BOUNDS_2D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_IN_BOUNDS_2D)));
	objectElement->SetKeyValue_StringKeys("POINT_IN_BOUNDS_3D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_IN_BOUNDS_3D)));
	objectElement->SetKeyValue_StringKeys("POINT_PERIODIC_1D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_PERIODIC_1D)));
	objectElement->SetKeyValue_StringKeys("POINT_PERIODIC_2D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_PERIODIC_2D)));
	objectElement->SetKeyValue_StringKeys("POINT_PERIODIC_3D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_PERIODIC_3D)));
	objectElement->SetKeyValue_StringKeys("POINT_REFLECTED_1D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_REFLECTED_1D)));
	objectElement->SetKeyValue_StringKeys("POINT_REFLECTED_2D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_REFLECTED_2D)));
	objectElement->SetKeyValue_StringKeys("POINT_REFLECTED_3D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_REFLECTED_3D)));
	objectElement->SetKeyValue_StringKeys("POINT_STOPPED_1D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_STOPPED_1D)));
	objectElement->SetKeyValue_StringKeys("POINT_STOPPED_2D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_STOPPED_2D)));
	objectElement->SetKeyValue_StringKeys("POINT_STOPPED_3D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_STOPPED_3D)));
	objectElement->SetKeyValue_StringKeys("POINT_UNIFORM_1D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_UNIFORM_1D)));
	objectElement->SetKeyValue_StringKeys("POINT_UNIFORM_2D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_UNIFORM_2D)));
	objectElement->SetKeyValue_StringKeys("POINT_UNIFORM_3D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_POINT_UNIFORM_3D)));
	objectElement->SetKeyValue_StringKeys("SET_SPATIAL_POS_1_1D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_SPATIAL_POS_1_1D)));
	objectElement->SetKeyValue_StringKeys("SET_SPATIAL_POS_1_2D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_SPATIAL_POS_1_2D)));
	objectElement->SetKeyValue_StringKeys("SET_SPATIAL_POS_1_3D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_SPATIAL_POS_1_3D)));
	objectElement->SetKeyValue_StringKeys("SET_SPATIAL_POS_2_1D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_SPATIAL_POS_2_1D)));
	objectElement->SetKeyValue_StringKeys("SET_SPATIAL_POS_2_2D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_SPATIAL_POS_2_2D)));
	objectElement->SetKeyValue_StringKeys("SET_SPATIAL_POS_2_3D", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SET_SPATIAL_POS_2_3D)));
	objectElement->SetKeyValue_StringKeys("SPATIAL_MAP_VALUE", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SPATIAL_MAP_VALUE)));
	
	objectElement->SetKeyValue_StringKeys("CLIPPEDINTEGRAL_1S", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_CLIPPEDINTEGRAL_1S)));
	objectElement->SetKeyValue_StringKeys("CLIPPEDINTEGRAL_2S", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_CLIPPEDINTEGRAL_2S)));
	//objectElement->SetKeyValue_StringKeys("CLIPPEDINTEGRAL_3S", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_CLIPPEDINTEGRAL_3S)));
	objectElement->SetKeyValue_StringKeys("DRAWBYSTRENGTH", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_DRAWBYSTRENGTH)));
	objectElement->SetKeyValue_StringKeys("INTNEIGHCOUNT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_INTNEIGHCOUNT)));
	objectElement->SetKeyValue_StringKeys("LOCALPOPDENSITY", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_LOCALPOPDENSITY)));
	objectElement->SetKeyValue_StringKeys("NEARESTINTNEIGH", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_NEARESTINTNEIGH)));
	objectElement->SetKeyValue_StringKeys("NEARESTNEIGH", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_NEARESTNEIGH)));
	objectElement->SetKeyValue_StringKeys("NEIGHCOUNT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_NEIGHCOUNT)));
	objectElement->SetKeyValue_StringKeys("TOTNEIGHSTRENGTH", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_TOTNEIGHSTRENGTH)));
	
	objectElement->SetKeyValue_StringKeys("AGE_INCR", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_AGE_INCR)));
	objectElement->SetKeyValue_StringKeys("DEFERRED_REPRO", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_DEFERRED_REPRO)));
	objectElement->SetKeyValue_StringKeys("WF_REPRO", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_WF_REPRO)));
	objectElement->SetKeyValue_StringKeys("FITNESS_ASEX_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_FITNESS_ASEX_1)));
	objectElement->SetKeyValue_StringKeys("FITNESS_ASEX_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_FITNESS_ASEX_2)));
	objectElement->SetKeyValue_StringKeys("FITNESS_ASEX_3", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_FITNESS_ASEX_3)));
	objectElement->SetKeyValue_StringKeys("FITNESS_SEX_1", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_FITNESS_SEX_1)));
	objectElement->SetKeyValue_StringKeys("FITNESS_SEX_2", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_FITNESS_SEX_2)));
	objectElement->SetKeyValue_StringKeys("FITNESS_SEX_3", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_FITNESS_SEX_3)));
	objectElement->SetKeyValue_StringKeys("MIGRANT_CLEAR", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_MIGRANT_CLEAR)));
	objectElement->SetKeyValue_StringKeys("SIMPLIFY_SORT_PRE", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SIMPLIFY_SORT_PRE)));
	objectElement->SetKeyValue_StringKeys("SIMPLIFY_SORT", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SIMPLIFY_SORT)));
	objectElement->SetKeyValue_StringKeys("SIMPLIFY_SORT_POST", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SIMPLIFY_SORT_POST)));
	objectElement->SetKeyValue_StringKeys("PARENTS_CLEAR", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_PARENTS_CLEAR)));
	objectElement->SetKeyValue_StringKeys("UNIQUE_MUTRUNS", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_UNIQUE_MUTRUNS)));
	objectElement->SetKeyValue_StringKeys("SURVIVAL", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(gEidos_OMP_threads_SURVIVAL)));
#endif
	
	objectElement->ContentsChanged("parallelGetTaskThreadCounts()");
	return result_SP;
}

//	(void)parallelSetNumThreads([Ni$ numThreads = NULL])
EidosValue_SP Eidos_ExecuteFunction_parallelSetNumThreads(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *numThreads_value = p_arguments[0].get();
	
	int64_t numThreads = gEidosMaxThreads;		// the default value, used for NULL
	
	if (numThreads_value->Type() == EidosValueType::kValueInt)
	{
		// An explicit override has been requested, even if numThreads == gEidosMaxThreads
		numThreads = numThreads_value->IntAtIndex(0, nullptr);
		gEidosNumThreadsOverride = true;
	}
	else
	{
		// The user has requested, with NULL, that the default thread usage pattern not be overridden
		gEidosNumThreadsOverride = false;
	}
	
	if (numThreads < 1)
		numThreads = 1;
	if (numThreads > gEidosMaxThreads)
		numThreads = gEidosMaxThreads;
	
	gEidosNumThreads = (int)numThreads;
	omp_set_num_threads((int)numThreads);
	
	// Note that this affects every running model, in SLiMgui.  Since we don't really support end users running SLiMgui
	// multithreaded, I'm not going to bother fixing that by saving/restoring it across SLiMgui context switches.
	
	return gStaticEidosValueVOID;
}

//	(void)parallelSetTaskThreadCounts(object$ dict)
EidosValue_SP Eidos_ExecuteFunction_parallelSetTaskThreadCounts(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *source_value = p_arguments[0].get();
	
	if (source_value->Type() == EidosValueType::kValueNULL)
	{
		// A dict value of NULL means "reset to the command-line default settings"
#ifdef _OPENMP
		_Eidos_SetOpenMPThreadCounts(gEidosDefaultPerTaskThreadCounts);
#endif
	}
	else
	{
		// Check that source is a subclass of EidosDictionaryUnretained.  We do this check here because we want to avoid making
		// EidosDictionaryUnretained visible in the public API; we want to pretend that there is just one class, Dictionary.
		// I'm not sure whether that's going to be right in the long term, but I want to keep my options open for now.
		EidosDictionaryUnretained *source = dynamic_cast<EidosDictionaryUnretained *>(source_value->ObjectElementAtIndex(0, nullptr));
		
		if (!source)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_parallelSetTaskThreadCounts): parallelSetTaskThreadCounts() can only take values from a Dictionary or a subclass of Dictionary." << EidosTerminate(nullptr);
		
		if (source->KeysAreStrings())
		{
			const EidosDictionaryHashTable_StringKeys *source_symbols = source->DictionarySymbols_StringKeys();
			const std::vector<std::string> source_keys = source->SortedKeys_StringKeys();
			
			if (source_symbols && source_symbols->size())
			{
				for (const std::string &key : source_keys)
				{
					auto kv_pair = source_symbols->find(key);
					const EidosValue_SP &value = kv_pair->second;
					
					if ((value->Type() == EidosValueType::kValueInt) && (value->Count() == 1))
					{
						int64_t value_int64 = value->IntAtIndex(0, nullptr);
						
						if ((value_int64 < 1) || (value_int64 > EIDOS_OMP_MAX_THREADS))
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_parallelSetTaskThreadCounts): parallelSetTaskThreadCounts() requires thread counts to be in [1, " << EIDOS_OMP_MAX_THREADS << "]." << EidosTerminate(nullptr);
						
#ifdef _OPENMP
						// We only actually process the key-value pairs when running multithreaded;
						// single-threaded, they are all ignored (for cross-compatibility).
						if (key == "ABS_FLOAT")							gEidos_OMP_threads_ABS_FLOAT = (int)value_int64;
						else if (key == "CEIL")							gEidos_OMP_threads_CEIL = (int)value_int64;
						else if (key == "EXP_FLOAT")					gEidos_OMP_threads_EXP_FLOAT = (int)value_int64;
						else if (key == "FLOOR")						gEidos_OMP_threads_FLOOR = (int)value_int64;
						else if (key == "LOG_FLOAT")					gEidos_OMP_threads_LOG_FLOAT = (int)value_int64;
						else if (key == "LOG10_FLOAT")					gEidos_OMP_threads_LOG10_FLOAT = (int)value_int64;
						else if (key == "LOG2_FLOAT")					gEidos_OMP_threads_LOG2_FLOAT = (int)value_int64;
						else if (key == "ROUND")						gEidos_OMP_threads_ROUND = (int)value_int64;
						else if (key == "SQRT_FLOAT")					gEidos_OMP_threads_SQRT_FLOAT = (int)value_int64;
						else if (key == "SUM_INTEGER")					gEidos_OMP_threads_SUM_INTEGER = (int)value_int64;
						else if (key == "SUM_FLOAT")					gEidos_OMP_threads_SUM_FLOAT = (int)value_int64;
						else if (key == "SUM_LOGICAL")					gEidos_OMP_threads_SUM_LOGICAL = (int)value_int64;
						else if (key == "TRUNC")						gEidos_OMP_threads_TRUNC = (int)value_int64;
						
						else if (key == "MAX_INT")						gEidos_OMP_threads_MAX_INT = (int)value_int64;
						else if (key == "MAX_FLOAT")					gEidos_OMP_threads_MAX_FLOAT = (int)value_int64;
						else if (key == "MIN_INT")						gEidos_OMP_threads_MIN_INT = (int)value_int64;
						else if (key == "MIN_FLOAT")					gEidos_OMP_threads_MIN_FLOAT = (int)value_int64;
						else if (key == "PMAX_INT_1")					gEidos_OMP_threads_PMAX_INT_1 = (int)value_int64;
						else if (key == "PMAX_INT_2")					gEidos_OMP_threads_PMAX_INT_2 = (int)value_int64;
						else if (key == "PMAX_FLOAT_1")					gEidos_OMP_threads_PMAX_FLOAT_1 = (int)value_int64;
						else if (key == "PMAX_FLOAT_2")					gEidos_OMP_threads_PMAX_FLOAT_2 = (int)value_int64;
						else if (key == "PMIN_INT_1")					gEidos_OMP_threads_PMIN_INT_1 = (int)value_int64;
						else if (key == "PMIN_INT_2")					gEidos_OMP_threads_PMIN_INT_2 = (int)value_int64;
						else if (key == "PMIN_FLOAT_1")					gEidos_OMP_threads_PMIN_FLOAT_1 = (int)value_int64;
						else if (key == "PMIN_FLOAT_2")					gEidos_OMP_threads_PMIN_FLOAT_2 = (int)value_int64;
						
						else if (key == "MATCH_INT")					gEidos_OMP_threads_MATCH_INT = (int)value_int64;
						else if (key == "MATCH_FLOAT")					gEidos_OMP_threads_MATCH_FLOAT = (int)value_int64;
						else if (key == "MATCH_STRING")					gEidos_OMP_threads_MATCH_STRING = (int)value_int64;
						else if (key == "MATCH_OBJECT")					gEidos_OMP_threads_MATCH_OBJECT = (int)value_int64;
						else if (key == "SAMPLE_INDEX")					gEidos_OMP_threads_SAMPLE_INDEX = (int)value_int64;
						else if (key == "SAMPLE_R_INT")					gEidos_OMP_threads_SAMPLE_R_INT = (int)value_int64;
						else if (key == "SAMPLE_R_FLOAT")				gEidos_OMP_threads_SAMPLE_R_FLOAT = (int)value_int64;
						else if (key == "SAMPLE_R_OBJECT")				gEidos_OMP_threads_SAMPLE_R_OBJECT = (int)value_int64;
						else if (key == "SAMPLE_WR_INT")				gEidos_OMP_threads_SAMPLE_WR_INT = (int)value_int64;
						else if (key == "SAMPLE_WR_FLOAT")				gEidos_OMP_threads_SAMPLE_WR_FLOAT = (int)value_int64;
						else if (key == "SAMPLE_WR_OBJECT")				gEidos_OMP_threads_SAMPLE_WR_OBJECT = (int)value_int64;
						else if (key == "TABULATE_MAXBIN")				gEidos_OMP_threads_TABULATE_MAXBIN = (int)value_int64;
						else if (key == "TABULATE")						gEidos_OMP_threads_TABULATE = (int)value_int64;
						
						else if (key == "CONTAINS_MARKER_MUT")			gEidos_OMP_threads_CONTAINS_MARKER_MUT = (int)value_int64;
						else if (key == "I_COUNT_OF_MUTS_OF_TYPE")		gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE = (int)value_int64;
						else if (key == "G_COUNT_OF_MUTS_OF_TYPE")		gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE = (int)value_int64;
						else if (key == "INDS_W_PEDIGREE_IDS")			gEidos_OMP_threads_INDS_W_PEDIGREE_IDS = (int)value_int64;
						else if (key == "RELATEDNESS")					gEidos_OMP_threads_RELATEDNESS = (int)value_int64;
						else if (key == "SAMPLE_INDIVIDUALS_1")			gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1 = (int)value_int64;
						else if (key == "SAMPLE_INDIVIDUALS_2")			gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2 = (int)value_int64;
						else if (key == "SET_FITNESS_SCALE_1")			gEidos_OMP_threads_SET_FITNESS_SCALE_1 = (int)value_int64;
						else if (key == "SET_FITNESS_SCALE_2")			gEidos_OMP_threads_SET_FITNESS_SCALE_2 = (int)value_int64;
						else if (key == "SUM_OF_MUTS_OF_TYPE")			gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE = (int)value_int64;
						
						else if (key == "DNORM_1")						gEidos_OMP_threads_DNORM_1 = (int)value_int64;
						else if (key == "DNORM_2")						gEidos_OMP_threads_DNORM_2 = (int)value_int64;
						else if (key == "RBINOM_1")						gEidos_OMP_threads_RBINOM_1 = (int)value_int64;
						else if (key == "RBINOM_2")						gEidos_OMP_threads_RBINOM_2 = (int)value_int64;
						else if (key == "RBINOM_3")						gEidos_OMP_threads_RBINOM_3 = (int)value_int64;
						else if (key == "RDUNIF_1")						gEidos_OMP_threads_RDUNIF_1 = (int)value_int64;
						else if (key == "RDUNIF_2")						gEidos_OMP_threads_RDUNIF_2 = (int)value_int64;
						else if (key == "RDUNIF_3")						gEidos_OMP_threads_RDUNIF_3 = (int)value_int64;
						else if (key == "REXP_1")						gEidos_OMP_threads_REXP_1 = (int)value_int64;
						else if (key == "REXP_2")						gEidos_OMP_threads_REXP_2 = (int)value_int64;
						else if (key == "RNORM_1")						gEidos_OMP_threads_RNORM_1 = (int)value_int64;
						else if (key == "RNORM_2")						gEidos_OMP_threads_RNORM_2 = (int)value_int64;
						else if (key == "RNORM_3")						gEidos_OMP_threads_RNORM_3 = (int)value_int64;
						else if (key == "RPOIS_1")						gEidos_OMP_threads_RPOIS_1 = (int)value_int64;
						else if (key == "RPOIS_2")						gEidos_OMP_threads_RPOIS_2 = (int)value_int64;
						else if (key == "RUNIF_1")						gEidos_OMP_threads_RUNIF_1 = (int)value_int64;
						else if (key == "RUNIF_2")						gEidos_OMP_threads_RUNIF_2 = (int)value_int64;
						else if (key == "RUNIF_3")						gEidos_OMP_threads_RUNIF_3 = (int)value_int64;
						
						else if (key == "SORT_INT")						gEidos_OMP_threads_SORT_INT = (int)value_int64;
						else if (key == "SORT_FLOAT")					gEidos_OMP_threads_SORT_FLOAT = (int)value_int64;
						else if (key == "SORT_STRING")					gEidos_OMP_threads_SORT_STRING = (int)value_int64;
						
						else if (key == "POINT_IN_BOUNDS_1D")			gEidos_OMP_threads_POINT_IN_BOUNDS_1D = (int)value_int64;
						else if (key == "POINT_IN_BOUNDS_2D")			gEidos_OMP_threads_POINT_IN_BOUNDS_2D = (int)value_int64;
						else if (key == "POINT_IN_BOUNDS_3D")			gEidos_OMP_threads_POINT_IN_BOUNDS_3D = (int)value_int64;
						else if (key == "POINT_PERIODIC_1D")			gEidos_OMP_threads_POINT_PERIODIC_1D = (int)value_int64;
						else if (key == "POINT_PERIODIC_2D")			gEidos_OMP_threads_POINT_PERIODIC_2D = (int)value_int64;
						else if (key == "POINT_PERIODIC_3D")			gEidos_OMP_threads_POINT_PERIODIC_3D = (int)value_int64;
						else if (key == "POINT_REFLECTED_1D")			gEidos_OMP_threads_POINT_REFLECTED_1D = (int)value_int64;
						else if (key == "POINT_REFLECTED_2D")			gEidos_OMP_threads_POINT_REFLECTED_2D = (int)value_int64;
						else if (key == "POINT_REFLECTED_3D")			gEidos_OMP_threads_POINT_REFLECTED_3D = (int)value_int64;
						else if (key == "POINT_STOPPED_1D")				gEidos_OMP_threads_POINT_STOPPED_1D = (int)value_int64;
						else if (key == "POINT_STOPPED_2D")				gEidos_OMP_threads_POINT_STOPPED_2D = (int)value_int64;
						else if (key == "POINT_STOPPED_3D")				gEidos_OMP_threads_POINT_STOPPED_3D = (int)value_int64;
						else if (key == "POINT_UNIFORM_1D")				gEidos_OMP_threads_POINT_UNIFORM_1D = (int)value_int64;
						else if (key == "POINT_UNIFORM_2D")				gEidos_OMP_threads_POINT_UNIFORM_2D = (int)value_int64;
						else if (key == "POINT_UNIFORM_3D")				gEidos_OMP_threads_POINT_UNIFORM_3D = (int)value_int64;
						else if (key == "SET_SPATIAL_POS_1_1D")			gEidos_OMP_threads_SET_SPATIAL_POS_1_1D = (int)value_int64;
						else if (key == "SET_SPATIAL_POS_1_2D")			gEidos_OMP_threads_SET_SPATIAL_POS_1_2D = (int)value_int64;
						else if (key == "SET_SPATIAL_POS_1_3D")			gEidos_OMP_threads_SET_SPATIAL_POS_1_3D = (int)value_int64;
						else if (key == "SET_SPATIAL_POS_2_1D")			gEidos_OMP_threads_SET_SPATIAL_POS_2_1D = (int)value_int64;
						else if (key == "SET_SPATIAL_POS_2_2D")			gEidos_OMP_threads_SET_SPATIAL_POS_2_2D = (int)value_int64;
						else if (key == "SET_SPATIAL_POS_2_3D")			gEidos_OMP_threads_SET_SPATIAL_POS_2_3D = (int)value_int64;
						else if (key == "SPATIAL_MAP_VALUE")			gEidos_OMP_threads_SPATIAL_MAP_VALUE = (int)value_int64;
						
						else if (key == "CLIPPEDINTEGRAL_1S")			gEidos_OMP_threads_CLIPPEDINTEGRAL_1S = (int)value_int64;
						else if (key == "CLIPPEDINTEGRAL_2S")			gEidos_OMP_threads_CLIPPEDINTEGRAL_2S = (int)value_int64;
						//else if (key == "CLIPPEDINTEGRAL_3S")			gEidos_OMP_threads_CLIPPEDINTEGRAL_3S = (int)value_int64;
						else if (key == "DRAWBYSTRENGTH")				gEidos_OMP_threads_DRAWBYSTRENGTH = (int)value_int64;
						else if (key == "INTNEIGHCOUNT")				gEidos_OMP_threads_INTNEIGHCOUNT = (int)value_int64;
						else if (key == "LOCALPOPDENSITY")				gEidos_OMP_threads_LOCALPOPDENSITY = (int)value_int64;
						else if (key == "NEARESTINTNEIGH")				gEidos_OMP_threads_NEARESTINTNEIGH = (int)value_int64;
						else if (key == "NEARESTNEIGH")					gEidos_OMP_threads_NEARESTNEIGH = (int)value_int64;
						else if (key == "NEIGHCOUNT")					gEidos_OMP_threads_NEIGHCOUNT = (int)value_int64;
						else if (key == "TOTNEIGHSTRENGTH")				gEidos_OMP_threads_TOTNEIGHSTRENGTH = (int)value_int64;
						
						else if (key == "AGE_INCR")						gEidos_OMP_threads_AGE_INCR = (int)value_int64;
						else if (key == "DEFERRED_REPRO")				gEidos_OMP_threads_DEFERRED_REPRO = (int)value_int64;
						else if (key == "WF_REPRO")						gEidos_OMP_threads_WF_REPRO = (int)value_int64;
						else if (key == "FITNESS_ASEX_1")				gEidos_OMP_threads_FITNESS_ASEX_1 = (int)value_int64;
						else if (key == "FITNESS_ASEX_2")				gEidos_OMP_threads_FITNESS_ASEX_2 = (int)value_int64;
						else if (key == "FITNESS_ASEX_3")				gEidos_OMP_threads_FITNESS_ASEX_3 = (int)value_int64;
						else if (key == "FITNESS_SEX_1")				gEidos_OMP_threads_FITNESS_SEX_1 = (int)value_int64;
						else if (key == "FITNESS_SEX_2")				gEidos_OMP_threads_FITNESS_SEX_2 = (int)value_int64;
						else if (key == "FITNESS_SEX_3")				gEidos_OMP_threads_FITNESS_SEX_3 = (int)value_int64;
						else if (key == "MIGRANT_CLEAR")				gEidos_OMP_threads_MIGRANT_CLEAR = (int)value_int64;
						else if (key == "SIMPLIFY_SORT_PRE")			gEidos_OMP_threads_SIMPLIFY_SORT_PRE = (int)value_int64;
						else if (key == "SIMPLIFY_SORT")				gEidos_OMP_threads_SIMPLIFY_SORT = (int)value_int64;
						else if (key == "SIMPLIFY_SORT_POST")			gEidos_OMP_threads_SIMPLIFY_SORT_POST = (int)value_int64;
						else if (key == "PARENTS_CLEAR")				gEidos_OMP_threads_PARENTS_CLEAR = (int)value_int64;
						else if (key == "UNIQUE_MUTRUNS")				gEidos_OMP_threads_UNIQUE_MUTRUNS = (int)value_int64;
						else if (key == "SURVIVAL")						gEidos_OMP_threads_SURVIVAL = (int)value_int64;
						else
							EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_parallelSetTaskThreadCounts): parallelSetTaskThreadCounts() does not recognize the task name " << key << "." << EidosTerminate(nullptr);
						
						// This assumes that any thread count set might push the maximum per-task thread count higher, but not lower
						gEidosPerTaskThreadCountsSetName = "UserDefined";
						gEidosPerTaskOriginalMaxThreadCount = std::max(gEidosPerTaskOriginalMaxThreadCount, (int)value_int64);
#endif
					}
					else
						EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_parallelSetTaskThreadCounts): parallelSetTaskThreadCounts() expects dict to contain singleton integer values." << EidosTerminate(nullptr);
				}
				
				// Clip all values to gEidosMaxThreads in preparation for use
#ifdef _OPENMP
				_Eidos_ClipOpenMPThreadCounts();
#endif
			}
		}
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_parallelSetTaskThreadCounts): parallelSetTaskThreadCounts() expects dict to use string keys." << EidosTerminate(nullptr);
	}
	
	return gStaticEidosValueVOID;
}

//	(void)rm([Ns variableNames = NULL])		// [logical$ removeConstants = F] removed in SLiM 4
EidosValue_SP Eidos_ExecuteFunction_rm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *variableNames_value = p_arguments[0].get();
	std::vector<std::string> symbols_to_remove;
	
	EidosSymbolTable &symbols = p_interpreter.SymbolTable();
	
	if (variableNames_value->Type() == EidosValueType::kValueNULL)
		symbols_to_remove = symbols.ReadWriteSymbols();
	else
	{
		int variableNames_count = variableNames_value->Count();
		
		for (int value_index = 0; value_index < variableNames_count; ++value_index)
			symbols_to_remove.emplace_back(variableNames_value->StringAtIndex(value_index, nullptr));
	}
	
	for (std::string &symbol : symbols_to_remove)
		symbols.RemoveValueForSymbol(EidosStringRegistry::GlobalStringIDForString(symbol));
	
	return gStaticEidosValueVOID;
}

//	(*)sapply(* x, string$ lambdaSource, [string$ simplify = "vector"])
EidosValue_SP Eidos_ExecuteFunction_sapply(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *x_value = p_arguments[0].get();
	int x_count = x_value->Count();
	
	// an empty x argument yields invisible NULL; this short-circuit is new but the behavior is the same as it was before,
	// except that we skip all the script tokenizing/parsing and so forth...
	if (x_count == 0)
		return gStaticEidosValueNULLInvisible;
	
	// Determine the simplification mode requested
	EidosValue_String *simplify_value = (EidosValue_String *)p_arguments[2].get();
	const std::string &simplify_string = simplify_value->StringRefAtIndex(0, nullptr);
	int simplify;
	
	if (simplify_string == "vector")		simplify = 0;
	else if (simplify_string == "matrix")	simplify = 1;
	else if (simplify_string == "match")	simplify = 2;
	else
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): unrecognized simplify option '" << simplify_string << "' in function sapply()." << EidosTerminate(nullptr);
	
	// Get the lambda string and cache its script
	EidosValue *lambda_value = p_arguments[1].get();
	EidosValue_String_singleton *lambda_value_singleton = dynamic_cast<EidosValue_String_singleton *>(p_arguments[1].get());
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
	
	// Execute inside try/catch so we can handle errors well
	std::vector<EidosValue_SP> results;
	
	gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, script, true};
	
	try
	{
		EidosSymbolTable &symbols = p_interpreter.SymbolTable();									// use our own symbol table
		EidosFunctionMap &function_map = p_interpreter.FunctionMap();								// use our own function map
		EidosInterpreter interpreter(*script, symbols, function_map, p_interpreter.Context(), p_interpreter.ExecutionOutputStream(), p_interpreter.ErrorOutputStream());
		bool null_included = false;				// has a NULL been seen among the return values
		bool consistent_return_length = true;	// consistent except for any NULLs returned
		int return_length = -1;					// what the consistent length is
		
		for (int value_index = 0; value_index < x_count; ++value_index)
		{
			EidosValue_SP apply_value = x_value->GetValueAtIndex(value_index, nullptr);
			
			// Set the iterator variable "applyValue" to the value
			symbols.SetValueForSymbolNoCopy(gEidosID_applyValue, std::move(apply_value));
			
			// Get the result.  BEWARE!  This calls causes re-entry into the Eidos interpreter, which is not usually
			// possible since Eidos does not support multithreaded usage.  This is therefore a key failure point for
			// bugs that would otherwise not manifest.
			EidosValue_SP &&return_value_SP = interpreter.EvaluateInterpreterBlock(false, true);		// do not print output, return the last statement value
			
			if (return_value_SP->Type() == EidosValueType::kValueVOID)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): each iteration within sapply() must return a non-void value." << EidosTerminate(nullptr);
			
			if (return_value_SP->Type() == EidosValueType::kValueNULL)
			{
				null_included = true;
			}
			else if (consistent_return_length)
			{
				int length = return_value_SP->Count();
				
				if (return_length == -1)
					return_length = length;
				else if (length != return_length)
					consistent_return_length = false;
			}
			
			results.emplace_back(return_value_SP);
		}
		
		// We do not want a leftover applyValue symbol in the symbol table, so we remove it now
		symbols.RemoveValueForSymbol(gEidosID_applyValue);
		
		// Assemble all the individual results together, just as c() does
		result_SP = ConcatenateEidosValues(results, true, false);	// allow NULL but not VOID
		
		// Finally, we restructure the results:
		//
		//	simplify == 0 ("vector") returns a plain vector, which is what we have already
		//	simplify == 1 ("matrix") returns a matrix with one column per return value
		//	simplify == 2 ("match") returns a matrix/array exactly matching the dimensions of x
		if (simplify == 1)
		{
			// A zero-length result is allowed and is returned verbatim
			if (result_SP->Count() > 0)
			{
				if (!consistent_return_length)
					EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): simplify = \"matrix\" was requested in function sapply(), but return values from lambdaSource were not of a consistent length." << EidosTerminate(nullptr);
				
				// one matrix column per return value, omitting NULLs; no need to reorder values to achieve this
				int64_t dim[2] = {return_length, result_SP->Count() / return_length};
				
				result_SP->SetDimensions(2, dim);
			}
		}
		else if (simplify == 2)
		{
			if (null_included)
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): simplify = \"match\" was requested in function sapply(), but return values included NULL." << EidosTerminate(nullptr);
			if (!consistent_return_length || (return_length != 1))
				EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_sapply): simplify = \"match\" was requested in function sapply(), but return values from lambdaSource were not all singletons." << EidosTerminate(nullptr);
			
			// match the dimensionality of x
			result_SP->CopyDimensionsFromValue(x_value);
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

//	(void)setSeed(integer$ seed)
EidosValue_SP Eidos_ExecuteFunction_setSeed(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *seed_value = p_arguments[0].get();
	
	Eidos_SetRNGSeed(seed_value->IntAtIndex(0, nullptr));
	
	return gStaticEidosValueVOID;
}

//	(void)stop([Ns$ message = NULL])
EidosValue_SP Eidos_ExecuteFunction_stop(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *message_value = p_arguments[0].get();
	
	if (message_value->Type() != EidosValueType::kValueNULL)
	{
		std::string &&stop_string = p_arguments[0]->StringAtIndex(0, nullptr);
		
		p_interpreter.ErrorOutputStream() << stop_string << std::endl;
		
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_stop): stop() called with error message:\n\n" << stop_string << EidosTerminate(nullptr);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_stop): stop() called." << EidosTerminate(nullptr);
	}
	
	// CODE COVERAGE: This is dead code
	return result_SP;
}

//	(logical$)suppressWarnings(logical$ suppress)
EidosValue_SP Eidos_ExecuteFunction_suppressWarnings(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *suppress_value = p_arguments[0].get();
	eidos_logical_t new_suppress = suppress_value->LogicalAtIndex(0, nullptr);
	eidos_logical_t old_suppress = gEidosSuppressWarnings;
	
	gEidosSuppressWarnings = new_suppress;
	
	return (old_suppress ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

//	(*)sysinfo(string$ key)
EidosValue_SP Eidos_ExecuteFunction_sysinfo(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *key_value = p_arguments[0].get();
	std::string key = key_value->StringAtIndex(0, nullptr);
	
	if (key == "os")
	{
#if defined(__APPLE__)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("macOS"));
#elif defined(_WIN32) || (_WIN64)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("Windows"));
#else
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("Unix"));		// assumed if we are not macOS or Windows
#endif
	}
	else if (key == "sysname")
	{
		struct utsname name;
		int ret = uname(&name);
		
		if (ret == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name.sysname));
	}
	else if (key == "release")
	{
		struct utsname name;
		int ret = uname(&name);
		
		if (ret == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name.release));
	}
	else if (key == "version")
	{
		struct utsname name;
		int ret = uname(&name);
		
		if (ret == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name.version));
	}
	else if (key == "nodename")
	{
		struct utsname name;
		int ret = uname(&name);
		
		if (ret == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name.nodename));
	}
	else if (key == "machine")
	{
		struct utsname name;
		int ret = uname(&name);
		
		if (ret == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name.machine));
	}
#if 0
	// "login" doesn't work on Windows, and "user" doesn't work on both Windows and Ubuntu 18.04; disabling both for now, nobody has asked for them anyway
	else if (key == "login")
	{
		char *name = getlogin();
		
		if (name)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name));
	}
	else if (key == "user")
	{
		uid_t uid = getuid();
		struct passwd *pwd = getpwuid(uid);
		
		if (pwd && pwd->pw_name)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(pwd->pw_name));
	}
#endif
	
	// if we fall through the here, the value is unknown
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("unknown"));
}

//	(string)system(string$ command, [string args = ""], [string input = ""], [logical$ stderr = F], [logical$ wait = T])
EidosValue_SP Eidos_ExecuteFunction_system(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	if (!Eidos_TemporaryDirectoryExists())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): in function system(), the temporary directory appears not to exist or is not writeable." << EidosTerminate(nullptr);
	
	EidosValue_String *command_value = (EidosValue_String *)p_arguments[0].get();
	EidosValue_String *args_value = (EidosValue_String *)p_arguments[1].get();
	int arg_count = args_value->Count();
	bool has_args = ((arg_count > 1) || ((arg_count == 1) && (args_value->StringRefAtIndex(0, nullptr).length() > 0)));
	EidosValue_String *input_value = (EidosValue_String *)p_arguments[2].get();
	int input_count = input_value->Count();
	bool has_input = ((input_count > 1) || ((input_count == 1) && (input_value->StringRefAtIndex(0, nullptr).length() > 0)));
	bool redirect_stderr = p_arguments[3]->LogicalAtIndex(0, nullptr);
	bool wait = p_arguments[4]->LogicalAtIndex(0, nullptr);
	
	// Construct the command string
	std::string command_string = command_value->StringRefAtIndex(0, nullptr);
	
	if (command_string.length() == 0)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): a non-empty command string must be supplied to system()." << EidosTerminate(nullptr);
	
	if (has_args)
	{
		for (int value_index = 0; value_index < arg_count; ++value_index)
		{
			command_string.append(" ");
			command_string.append(args_value->StringRefAtIndex(value_index, nullptr));
		}
	}
	
	// Make the input temporary file and redirect, if requested
	if (has_input)
	{
		// thanks to http://stackoverflow.com/questions/499636/how-to-create-a-stdofstream-to-a-temp-file for the temp file creation code
		
		std::string name_string = Eidos_TemporaryDirectory() + "eidos_system_XXXXXX";
		char *name = strdup(name_string.c_str());
		int fd = mkstemp(name);
		
		if (fd == -1)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) mkstemp() failed!" << EidosTerminate(nullptr);
		
		std::ofstream file_stream(name, std::ios_base::out);
		close(fd);	// opened by mkstemp()
		
		if (!file_stream.is_open())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) ofstream() failed!" << EidosTerminate(nullptr);
		
		if (input_count == 1)
		{
			file_stream << input_value->StringRefAtIndex(0, nullptr);	// no final newline in this case, so the user can precisely specify the file contents if desired
		}
		else
		{
			const std::vector<std::string> &string_vec = *input_value->StringVector();
			
			for (int value_index = 0; value_index < input_count; ++value_index)
			{
				file_stream << string_vec[value_index];
				
				// Add newlines after all lines, including the last
				file_stream << std::endl;
			}
		}
		
		if (file_stream.bad())
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) stream errors writing temporary file for input" << EidosTerminate(nullptr);
		
		command_string.append(" < ");
		command_string.append(name);
		
		free(name);
	}
	
	// Redirect standard error, if requested
	if (redirect_stderr)
	{
		command_string.append(" 2>&1");
	}
	
	// Run in the background, if requested
	if (!wait)
	{
		command_string.append(" &");
	}
	
	// Determine whether we are running in the background, as indicated by a command line ending in " &"
	if ((command_string.length() > 2) && (command_string.substr(command_string.length() - 2, std::string::npos) == " &"))
	{
		wait = false;
	}
	
	if (wait)
	{
		// Execute the command string; thanks to http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
		//std::cout << "Executing command string: " << command_string << std::endl;
		//std::cout << "Command string length: " << command_string.length() << " bytes" << std::endl;
		
		char buffer[128];
		std::string result = "";
		std::shared_ptr<FILE> command_pipe(popen(command_string.c_str(), "r"), pclose);
		if (!command_pipe)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) popen() failed!" << EidosTerminate(nullptr);
		while (!feof(command_pipe.get())) {
			if (fgets(buffer, 128, command_pipe.get()) != NULL)
				result += buffer;
		}
		
		// Parse the result into lines and make a result vector
		std::istringstream result_stream(result);
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		std::string line;
		
		while (getline(result_stream, line))
			string_result->PushString(line);
		
		return result_SP;
	}
	else
	{
		// Execute the command string without collecting output, hopefully in the background with an immediate return from system()
		int ret = system(command_string.c_str());
		
		if (ret != 0)
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_system): (internal error) system() failed with return code " << ret << "." << EidosTerminate(nullptr);
		
		return gStaticEidosValue_String_ZeroVec;
	}
}

//	(string$)time(void)
EidosValue_SP Eidos_ExecuteFunction_time(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	time_t rawtime;
	struct tm timeinfo;
	char buffer[20];		// should never be more than 8, in fact, plus a null
	
	time(&rawtime);
	localtime_r(&rawtime, &timeinfo);
	strftime(buffer, 20, "%H:%M:%S", &timeinfo);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(std::string(buffer)));
	
	return result_SP;
}

//	(float$)usage([ls$ type = "rss"])
EidosValue_SP Eidos_ExecuteFunction_usage(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *type_value = p_arguments[0].get();
	size_t usage = 0;
	
	if (type_value->Type() == EidosValueType::kValueLogical)
	{
		// old-style API, through SLiM 4.0.1 (but still supported): logical value, F == current RSS, T == peak RSS
		bool peak = type_value->LogicalAtIndex(0, nullptr);
		
		usage = (peak ? Eidos_GetPeakRSS() : Eidos_GetCurrentRSS());
	}
	else
	{
		// new-style API, after SLiM 4.0.1: string value, "rss" == current RSS, "rss_peak" = peak RSS, "vm" = current VM
		std::string type = type_value->StringAtIndex(0, nullptr);
		
		if (type == "rss")
			usage = Eidos_GetCurrentRSS();
		else if (type == "rss_peak")
			usage = Eidos_GetPeakRSS();
		else if (type == "vm")
			usage = Eidos_GetVMUsage();
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_usage): usage() did not recognize the requested type, '" << type << "'; type should be 'rss', 'rss_peak', or 'vm'." << EidosTerminate(nullptr);
	}
	
	double usage_MB = usage / (1024.0 * 1024.0);
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(usage_MB));
	
	return result_SP;
}

//	(void)version([logical$ print = T])
EidosValue_SP Eidos_ExecuteFunction_version(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	bool print = p_arguments[0]->LogicalAtIndex(0, nullptr);
	
	if (print)
	{
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		output_stream << "Eidos version " << EIDOS_VERSION_STRING << std::endl;
		
		if (gEidosContextVersionString.length())
			output_stream << gEidosContextVersionString << std::endl;
	}
	
	// Return the versions as floats
	EidosValue_Float_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->reserve(2);
	result_SP = EidosValue_SP(result);
	
	result->push_float_no_check(EIDOS_VERSION_FLOAT);
	
	if (gEidosContextVersion != 0.0)
		result->push_float_no_check(gEidosContextVersion);
	
	if (print)
		result->SetInvisible(true);
	
	return result_SP;
}

// (void)_startBenchmark(string$ type)
EidosValue_SP SLiM_ExecuteFunction__startBenchmark(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *type_value = p_arguments[0].get();
	
	if (gEidosBenchmarkType != EidosBenchmarkType::kNone)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction__startBenchmark): benchmarking has already been started." << EidosTerminate();
	
	std::string type = type_value->StringAtIndex(0, nullptr);
	
	if (type == "SAMPLE_INDEX")				gEidosBenchmarkType = EidosBenchmarkType::k_SAMPLE_INDEX;
	else if (type == "TABULATE_MAXBIN")		gEidosBenchmarkType = EidosBenchmarkType::k_TABULATE_MAXBIN;
	else if (type == "AGE_INCR")			gEidosBenchmarkType = EidosBenchmarkType::k_AGE_INCR;
	else if (type == "DEFERRED_REPRO")		gEidosBenchmarkType = EidosBenchmarkType::k_DEFERRED_REPRO;
	else if (type == "WF_REPRO")			gEidosBenchmarkType = EidosBenchmarkType::k_WF_REPRO;
	else if (type == "FITNESS_ASEX_1")		gEidosBenchmarkType = EidosBenchmarkType::k_FITNESS_ASEX_1;
	else if (type == "FITNESS_ASEX_2")		gEidosBenchmarkType = EidosBenchmarkType::k_FITNESS_ASEX_2;
	else if (type == "FITNESS_ASEX_3")		gEidosBenchmarkType = EidosBenchmarkType::k_FITNESS_ASEX_3;
	else if (type == "FITNESS_SEX_1")		gEidosBenchmarkType = EidosBenchmarkType::k_FITNESS_SEX_1;
	else if (type == "FITNESS_SEX_2")		gEidosBenchmarkType = EidosBenchmarkType::k_FITNESS_SEX_2;
	else if (type == "FITNESS_SEX_3")		gEidosBenchmarkType = EidosBenchmarkType::k_FITNESS_SEX_3;
	else if (type == "MIGRANT_CLEAR")		gEidosBenchmarkType = EidosBenchmarkType::k_MIGRANT_CLEAR;
	else if (type == "SIMPLIFY_SORT_PRE")	gEidosBenchmarkType = EidosBenchmarkType::k_SIMPLIFY_SORT_PRE;
	else if (type == "SIMPLIFY_SORT")		gEidosBenchmarkType = EidosBenchmarkType::k_SIMPLIFY_SORT;
	else if (type == "SIMPLIFY_SORT_POST")	gEidosBenchmarkType = EidosBenchmarkType::k_SIMPLIFY_SORT_POST;
	else if (type == "PARENTS_CLEAR")		gEidosBenchmarkType = EidosBenchmarkType::k_PARENTS_CLEAR;
	else if (type == "UNIQUE_MUTRUNS")		gEidosBenchmarkType = EidosBenchmarkType::k_UNIQUE_MUTRUNS;
	else if (type == "SURVIVAL")			gEidosBenchmarkType = EidosBenchmarkType::k_SURVIVAL;
	else if (type == "MUT_TALLY")			gEidosBenchmarkType = EidosBenchmarkType::k_MUT_TALLY;
	else if (type == "MUTRUN_FREE")			gEidosBenchmarkType = EidosBenchmarkType::k_MUTRUN_FREE;
	else if (type == "MUT_FREE")			gEidosBenchmarkType = EidosBenchmarkType::k_MUT_FREE;
	else if (type == "SIMPLIFY_CORE")		gEidosBenchmarkType = EidosBenchmarkType::k_SIMPLIFY_CORE;
	else
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction__startBenchmark): unrecognized benchmark type " << type << "." << EidosTerminate();
	
	gEidosBenchmarkAccumulator = 0;
	
	return gStaticEidosValueVOID;
}

// (float$)_stopBenchmark(void)
EidosValue_SP SLiM_ExecuteFunction__stopBenchmark(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	if (gEidosBenchmarkType == EidosBenchmarkType::kNone)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction__stopBenchmark): benchmarking has not been started." << EidosTerminate();
	
	double benchmark_time = Eidos_ElapsedProfileTime(gEidosBenchmarkAccumulator);
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(benchmark_time));
	
	// reset so a new benchmark can be started
	gEidosBenchmarkType = EidosBenchmarkType::kNone;
	gEidosBenchmarkAccumulator = 0;
	
	return result_SP;
}







































































