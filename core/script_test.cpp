//
//  script_test.cpp
//  SLiM
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "script_test.h"
#include "script.h"
#include "script_value.h"
#include "script_interpreter.h"
#include "slim_global.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>


using std::string;
using std::vector;
using std::map;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


// Helper functions for testing
void AssertScriptSuccess(string p_script_string, ScriptValue *p_correct_result);
void AssertScriptRaise(string p_script_string);


// Instantiates and runs the script, and prints an error if the result does not match expectations
void AssertScriptSuccess(string p_script_string, ScriptValue *p_correct_result)
{
	Script script(1, 1, p_script_string, 0);
	ScriptValue *result = nullptr;
	
	try {
		script.Tokenize();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during Tokenize(): " << GetTrimmedRaiseMessage() << endl;
		return;
	}
	
	try {
		script.ParseInterpreterBlockToAST();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during ParseToAST(): " << GetTrimmedRaiseMessage() << endl;
		return;
	}
	
	try {
		ScriptInterpreter interpreter(script);
		
		result = interpreter.EvaluateInterpreterBlock();
		
		// We have to copy the result; it lives in the interpreter's symbol table, which will be deleted when we leave this local scope
		result = result->CopyValues();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during EvaluateInterpreterBlock(): " << GetTrimmedRaiseMessage() << endl;
		return;
	}
	
	// Check that the result is actually what we want it to be
	if (result == nullptr)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : return value is nullptr" << endl;
		return;
	}
	if (result->Type() != p_correct_result->Type())
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : unexpected return type (" << result->Type() << ", expected " << p_correct_result->Type() << ")" << endl;
		return;
	}
	if (result->Count() != p_correct_result->Count())
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : unexpected return length (" << result->Count() << ", expected " << p_correct_result->Count() << ")" << endl;
		return;
	}
	
	for (int value_index = 0; value_index < result->Count(); ++value_index)
	{
		if (CompareScriptValues(result, value_index, p_correct_result, value_index) != 0)
		{
			std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : mismatched values (" << *result << "), expected (" << *p_correct_result << ")" << endl;
			return;
		}
	}
	
	std::cerr << p_script_string << " == " << p_correct_result->Type() << "(" << *p_correct_result << ") : \e[32mSUCCESS\e[0m" << endl;
}

// Instantiates and runs the script, and prints an error if the script does not cause an exception to be raised
void AssertScriptRaise(string p_script_string)
{
	Script script(1, 1, p_script_string, 0);
	
	try {
		script.Tokenize();
		script.ParseInterpreterBlockToAST();
		
		ScriptInterpreter interpreter(script);
		
		interpreter.EvaluateInterpreterBlock();
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : no raise during EvaluateInterpreterBlock()." << endl;
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " == (expected raise) " << GetTrimmedRaiseMessage() << " : \e[32mSUCCESS\e[0m" << endl;
		return;
	}
}

void RunSLiMScriptTests(void)
{
	// test literals, built-in identifiers, and tokenization
	AssertScriptSuccess("3;", new ScriptValue_Int(3));
	AssertScriptSuccess("3e2;", new ScriptValue_Int(300));
	AssertScriptSuccess("3.1;", new ScriptValue_Float(3.1));
	AssertScriptSuccess("3.1e2;", new ScriptValue_Float(3.1e2));
	AssertScriptSuccess("3.1e-2;", new ScriptValue_Float(3.1e-2));
	AssertScriptSuccess("\"foo\";", new ScriptValue_String("foo"));
	AssertScriptSuccess("\"foo\\tbar\";", new ScriptValue_String("foo\tbar"));
	AssertScriptSuccess("T;", new ScriptValue_Logical(true));
	AssertScriptSuccess("F;", new ScriptValue_Logical(false));
	AssertScriptRaise("$foo;");
	
	// test vector-to-singleton comparisons for integers
	AssertScriptSuccess("rep(1:3, 2) == 2;", new ScriptValue_Logical(false, true, false, false, true, false));
	AssertScriptSuccess("rep(1:3, 2) != 2;", new ScriptValue_Logical(true, false, true, true, false, true));
	AssertScriptSuccess("rep(1:3, 2) < 2;", new ScriptValue_Logical(true, false, false, true, false, false));
	AssertScriptSuccess("rep(1:3, 2) <= 2;", new ScriptValue_Logical(true, true, false, true, true, false));
	AssertScriptSuccess("rep(1:3, 2) > 2;", new ScriptValue_Logical(false, false, true, false, false, true));
	AssertScriptSuccess("rep(1:3, 2) >= 2;", new ScriptValue_Logical(false, true, true, false, true, true));
	
	AssertScriptSuccess("2 == rep(1:3, 2);", new ScriptValue_Logical(false, true, false, false, true, false));
	AssertScriptSuccess("2 != rep(1:3, 2);", new ScriptValue_Logical(true, false, true, true, false, true));
	AssertScriptSuccess("2 > rep(1:3, 2);", new ScriptValue_Logical(true, false, false, true, false, false));
	AssertScriptSuccess("2 >= rep(1:3, 2);", new ScriptValue_Logical(true, true, false, true, true, false));
	AssertScriptSuccess("2 < rep(1:3, 2);", new ScriptValue_Logical(false, false, true, false, false, true));
	AssertScriptSuccess("2 <= rep(1:3, 2);", new ScriptValue_Logical(false, true, true, false, true, true));
}































































