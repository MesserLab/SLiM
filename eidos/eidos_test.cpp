//
//  eidos_test.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#include "eidos_test.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_interpreter.h"
#include "eidos_global.h"
#include "eidos_rng.h"

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
void EidosAssertScriptSuccess(const string &p_script_string, EidosValue *p_correct_result);
void EidosAssertScriptRaise(const string &p_script_string, const int p_bad_position, const std::string &p_reason_snip);

// Keeping records of test success / failure
static int gEidosTestSuccessCount = 0;
static int gEidosTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void EidosAssertScriptSuccess(const string &p_script_string, EidosValue *p_correct_result)
{
	EidosScript script(p_script_string);
	EidosValue *result = nullptr;
	EidosSymbolTable symbol_table;
	
	gEidosCurrentScript = &script;
	
	gEidosTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	try {
		script.Tokenize();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during Tokenize(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		script.ParseInterpreterBlockToAST();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during ParseToAST(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		EidosInterpreter interpreter(script, symbol_table, nullptr);
		
		// note InjectIntoInterpreter() is not called here; we want a pristine environment to test the language itself
		
		result = interpreter.EvaluateInterpreterBlock(true);
		
		// We have to copy the result; it lives in the interpreter's symbol table, which will be deleted when we leave this local scope
		result = result->CopyValues();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during EvaluateInterpreterBlock(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	// Check that the result is actually what we want it to be
	if (result == nullptr)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : return value is nullptr" << endl;
	}
	else if (result->Type() != p_correct_result->Type())
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : unexpected return type (" << result->Type() << ", expected " << p_correct_result->Type() << ")" << endl;
	}
	else if (result->Count() != p_correct_result->Count())
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : unexpected return length (" << result->Count() << ", expected " << p_correct_result->Count() << ")" << endl;
	}
	else
	{
		for (int value_index = 0; value_index < result->Count(); ++value_index)
		{
			if (CompareEidosValues(result, value_index, p_correct_result, value_index, nullptr) != 0)
			{
				std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : mismatched values (" << *result << "), expected (" << *p_correct_result << ")" << endl;
				return;
			}
		}
		
		gEidosTestFailureCount--;	// correct for our assumption of failure above
		gEidosTestSuccessCount++;
		
		//std::cerr << p_script_string << " == " << p_correct_result->Type() << "(" << *p_correct_result << ") : \e[32mSUCCESS\e[0m" << endl;
	}
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

// Instantiates and runs the script, and prints an error if the script does not cause an exception to be raised
void EidosAssertScriptRaise(const string &p_script_string, const int p_bad_position, const std::string &p_reason_snip)
{
	EidosScript script(p_script_string);
	EidosSymbolTable symbol_table;
	
	gEidosCurrentScript = &script;
	
	try {
		script.Tokenize();
		script.ParseInterpreterBlockToAST();
		
		EidosInterpreter interpreter(script, symbol_table, nullptr);
		
		// note InjectIntoInterpreter() is not called here; we want a pristine environment to test the language itself
		
		interpreter.EvaluateInterpreterBlock(true);
		
		gEidosTestFailureCount++;
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : no raise during EvaluateInterpreterBlock()." << endl;
	}
	catch (std::runtime_error err)
	{
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
		if (raise_message.find(p_reason_snip) != string::npos)
		{
			if ((gEidosCharacterStartOfError == -1) || (gEidosCharacterEndOfError == -1) || !gEidosCurrentScript)
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but no error info set" << endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else if (gEidosCharacterStartOfError != p_bad_position)
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but error position unexpected" << endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << endl;
				eidos_log_script_error(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else
			{
				gEidosTestSuccessCount++;
				
				//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : \e[32mSUCCESS\e[0m" << endl;
			}
		}
		else
		{
			gEidosTestFailureCount++;
			
			std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise message mismatch (expected \"" << p_reason_snip << "\")." << endl;
			std::cerr << "   raise message: " << raise_message << endl;
			std::cerr << "--------------------" << std::endl << std::endl;
		}
	}
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

void RunEidosTests(void)
{
	// Reset error counts
	gEidosTestSuccessCount = 0;
	gEidosTestFailureCount = 0;
	
	// test literals, built-in identifiers, and tokenization
	#pragma mark literals & identifiers
	EidosAssertScriptSuccess("3;", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("3e2;", new EidosValue_Int_singleton(300));
	EidosAssertScriptSuccess("3.1;", new EidosValue_Float_singleton(3.1));
	EidosAssertScriptSuccess("3.1e2;", new EidosValue_Float_singleton(3.1e2));
	EidosAssertScriptSuccess("3.1e-2;", new EidosValue_Float_singleton(3.1e-2));
	EidosAssertScriptSuccess("3.1e+2;", new EidosValue_Float_singleton(3.1e+2));
	EidosAssertScriptSuccess("'foo';", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("'foo\\tbar';", new EidosValue_String_singleton("foo\tbar"));
	EidosAssertScriptSuccess("'\\'foo\\'\\t\\\"bar\"';", new EidosValue_String_singleton("'foo'\t\"bar\""));
	EidosAssertScriptSuccess("\"foo\";", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("\"foo\\tbar\";", new EidosValue_String_singleton("foo\tbar"));
	EidosAssertScriptSuccess("\"\\'foo'\\t\\\"bar\\\"\";", new EidosValue_String_singleton("'foo'\t\"bar\""));
	EidosAssertScriptSuccess("T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("INF;", new EidosValue_Float_singleton(std::numeric_limits<double>::infinity()));
	EidosAssertScriptSuccess("NAN;", new EidosValue_Float_singleton(std::numeric_limits<double>::quiet_NaN()));
	EidosAssertScriptSuccess("E - exp(1) < 0.0000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("PI - asin(1)*2 < 0.0000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("foo$foo;", 3, "unrecognized token");
	EidosAssertScriptRaise("3..5;", 3, "unexpected token");		// second period is a dot operator!
	EidosAssertScriptRaise("3ee5;", 0, "unrecognized token");
	EidosAssertScriptRaise("3e-+5;", 0, "unrecognized token");
	EidosAssertScriptRaise("3e-;", 0, "unrecognized token");
	EidosAssertScriptRaise("3e;", 0, "unrecognized token");
	EidosAssertScriptRaise("'foo' + 'foo;", 8, "unexpected EOF");
	EidosAssertScriptRaise("'foo' + 'foo\\q';", 12, "illegal escape");
	EidosAssertScriptRaise("'foo' + 'foo\\", 8, "unexpected EOF");
	EidosAssertScriptRaise("'foo' + 'foo\n';", 8, "illegal newline");
	EidosAssertScriptRaise("1e100;", 0, "could not be represented");							// out of range for integer
	EidosAssertScriptRaise("1000000000000000000000000000;", 0, "could not be represented");		// out of range for integer
	EidosAssertScriptRaise("1.0e100000000000;", 0, "could not be represented");					// out of range for double
	EidosAssertScriptRaise("T = 5;", 2, "is a constant");
	EidosAssertScriptRaise("F = 5;", 2, "is a constant");
	EidosAssertScriptRaise("NULL = 5;", 5, "is a constant");
	EidosAssertScriptRaise("INF = 5;", 4, "is a constant");
	EidosAssertScriptRaise("NAN = 5;", 4, "is a constant");
	EidosAssertScriptRaise("E = 5;", 2, "is a constant");
	EidosAssertScriptRaise("PI = 5;", 3, "is a constant");
	
	// test some simple parsing errors
	#pragma mark parsing
	EidosAssertScriptRaise("5 + 5", 5, "unexpected token");					// missing ;
	EidosAssertScriptRaise("{ 5;", 4, "unexpected token");					// missing }
	EidosAssertScriptRaise("5 };", 2, "unexpected token");					// missing {
	EidosAssertScriptRaise("(5 + 7;", 6, "unexpected token");				// missing )
	EidosAssertScriptRaise("5 + 7);", 5, "unexpected token");				// missing (
	EidosAssertScriptRaise("a[5;", 3, "unexpected token");					// missing ]
	EidosAssertScriptRaise("a 5];", 2, "unexpected token");					// missing ]
	EidosAssertScriptRaise("a(5;", 3, "unexpected token");					// missing )
	EidosAssertScriptRaise("a 5);", 2, "unexpected token");					// missing (
	EidosAssertScriptRaise("a.;", 2, "unexpected token");					// missing identifier
	EidosAssertScriptRaise("if (5 T;", 6, "unexpected token");				// missing )
	EidosAssertScriptRaise("if 5) T;", 3, "unexpected token");				// missing (
	EidosAssertScriptRaise("if (5) else 5;", 7, "unexpected token");		// missing statement
	EidosAssertScriptRaise("do ; (T);", 5, "unexpected token");				// missing while
	EidosAssertScriptRaise("do ; while T);", 11, "unexpected token");		// missing (
	EidosAssertScriptRaise("do ; while (T;", 13, "unexpected token");		// missing )
	EidosAssertScriptRaise("while T);", 6, "unexpected token");				// missing (
	EidosAssertScriptRaise("while (T;", 8, "unexpected token");				// missing )
	EidosAssertScriptRaise("for;", 3, "unexpected token");					// missing range
	EidosAssertScriptRaise("for (x);", 6, "unexpected token");				// missing in
	EidosAssertScriptRaise("for (x in);", 9, "unexpected token");			// missing range
	EidosAssertScriptRaise("for (in 3:5);", 5, "unexpected token");			// missing range variable
	EidosAssertScriptRaise("for (x in 3:5;", 13, "unexpected token");		// missing )
	EidosAssertScriptRaise("for x in 3:5) ;", 4, "unexpected token");		// missing (
	EidosAssertScriptRaise("next 5;", 5, "unexpected token");				// missing ;
	EidosAssertScriptRaise("break 5;", 6, "unexpected token");				// missing ;
	
	// test some simple runtime errors
	#pragma mark runtime
	EidosAssertScriptRaise("x = y * 3;", 4, "undefined identifier");									// undefined variable referenced
	EidosAssertScriptRaise("print(y * 3);", 6, "undefined identifier");									// undefined variable referenced as function argument
	
	EidosAssertScriptRaise("x = T; x[1];", 8, "out of range");									// subscript out of range (singleton logical)
	EidosAssertScriptRaise("x = T; x[-1];", 8, "out of range");									// subscript out of range (singleton logical)
	EidosAssertScriptRaise("x = T; x[1] = T;", 8, "out-of-range index");								// subscript out of range in assignment (singleton logical)
	EidosAssertScriptRaise("x = T; x[-1] = T;", 8, "out-of-range index");								// subscript out of range in assignment (singleton logical)
	EidosAssertScriptRaise("x = c(T,F); x[2];", 13, "out of range");							// subscript out of range (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[-1];", 13, "out of range");							// subscript out of range (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[2] = F;", 13, "out-of-range index");						// subscript out of range in assignment (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[-1] = F;", 13, "out-of-range index");						// subscript out of range in assignment (vector logical)

	EidosAssertScriptRaise("x = 8; x[1];", 8, "out of range");									// subscript out of range (singleton int)
	EidosAssertScriptRaise("x = 8; x[-1];", 8, "out of range");									// subscript out of range (singleton int)
	EidosAssertScriptRaise("x = 8; x[1] = 7;", 8, "out-of-range index");								// subscript out of range in assignment (singleton int)
	EidosAssertScriptRaise("x = 8; x[-1] = 7;", 8, "out-of-range index");								// subscript out of range in assignment (singleton int)
	EidosAssertScriptRaise("x = 7:9; x[3];", 10, "out of range");								// subscript out of range (vector int)
	EidosAssertScriptRaise("x = 7:9; x[-1];", 10, "out of range");								// subscript out of range (vector int)
	EidosAssertScriptRaise("x = 7:9; x[3] = 12;", 10, "out-of-range index");							// subscript out of range in assignment (vector int)
	EidosAssertScriptRaise("x = 7:9; x[-1] = 12;", 10, "out-of-range index");							// subscript out of range in assignment (vector int)

	EidosAssertScriptRaise("x = 8.0; x[1];", 10, "out of range");								// subscript out of range (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[-1];", 10, "out of range");								// subscript out of range (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[1] = 7.0;", 10, "out-of-range index");							// subscript out of range in assignment (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[-1] = 7.0;", 10, "out-of-range index");						// subscript out of range in assignment (singleton float)
	EidosAssertScriptRaise("x = 7.0:9; x[3];", 12, "out of range");								// subscript out of range (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[-1];", 12, "out of range");							// subscript out of range (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[3] = 12.0;", 12, "out-of-range index");						// subscript out of range in assignment (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[-1] = 12.0;", 12, "out-of-range index");						// subscript out of range in assignment (vector float)

	EidosAssertScriptRaise("x = 'foo'; x[1];", 12, "out of range");								// subscript out of range (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[-1];", 12, "out of range");							// subscript out of range (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[1] = _Test(6);", 12, "out-of-range index");					// subscript out of range in assignment (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[-1] = _Test(6);", 12, "out-of-range index");					// subscript out of range in assignment (singleton string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[2];", 22, "out of range");					// subscript out of range (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[-1];", 22, "out of range");					// subscript out of range (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[2] = _Test(6);", 22, "out-of-range index");		// subscript out of range in assignment (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[-1] = _Test(6);", 22, "out-of-range index");		// subscript out of range in assignment (vector string)

	EidosAssertScriptRaise("x = _Test(8); x[1];", 15, "out of range");							// subscript out of range (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[-1];", 15, "out of range");							// subscript out of range (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[1] = _Test(6);", 15, "out-of-range index");				// subscript out of range in assignment (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[-1] = _Test(6);", 15, "out-of-range index");				// subscript out of range in assignment (singleton object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[2];", 23, "out of range");					// subscript out of range (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[-1];", 23, "out of range");					// subscript out of range (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[2] = _Test(6);", 23, "out-of-range index");		// subscript out of range in assignment (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[-1] = _Test(6);", 23, "out-of-range index");		// subscript out of range in assignment (vector object)

	
	// ************************************************************************************
	//
	//	Operator tests
	//
	
	// test vector-to-singleton comparisons for integers
	#pragma mark vectors & singletons
	EidosAssertScriptSuccess("rep(1:3, 2) == 2;", new EidosValue_Logical{false, true, false, false, true, false});
	EidosAssertScriptSuccess("rep(1:3, 2) != 2;", new EidosValue_Logical{true, false, true, true, false, true});
	EidosAssertScriptSuccess("rep(1:3, 2) < 2;", new EidosValue_Logical{true, false, false, true, false, false});
	EidosAssertScriptSuccess("rep(1:3, 2) <= 2;", new EidosValue_Logical{true, true, false, true, true, false});
	EidosAssertScriptSuccess("rep(1:3, 2) > 2;", new EidosValue_Logical{false, false, true, false, false, true});
	EidosAssertScriptSuccess("rep(1:3, 2) >= 2;", new EidosValue_Logical{false, true, true, false, true, true});
	
	EidosAssertScriptSuccess("2 == rep(1:3, 2);", new EidosValue_Logical{false, true, false, false, true, false});
	EidosAssertScriptSuccess("2 != rep(1:3, 2);", new EidosValue_Logical{true, false, true, true, false, true});
	EidosAssertScriptSuccess("2 > rep(1:3, 2);", new EidosValue_Logical{true, false, false, true, false, false});
	EidosAssertScriptSuccess("2 >= rep(1:3, 2);", new EidosValue_Logical{true, true, false, true, true, false});
	EidosAssertScriptSuccess("2 < rep(1:3, 2);", new EidosValue_Logical{false, false, true, false, false, true});
	EidosAssertScriptSuccess("2 <= rep(1:3, 2);", new EidosValue_Logical{false, true, true, false, true, true});
	
	#pragma mark -
	#pragma mark Operators
	#pragma mark -
	
	// operator +
	#pragma mark operator +
	EidosAssertScriptRaise("NULL+T;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0.5;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+'foo';", 4, "does not support operands of type NULL");
	EidosAssertScriptRaise("NULL+_Test(7);", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+(0:2);", 4, "combination of operand types");
	EidosAssertScriptRaise("T+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0.5+NULL;", 3, "combination of operand types");
	EidosAssertScriptRaise("'foo'+NULL;", 5, "does not support operands of type NULL");
	EidosAssertScriptRaise("_Test(7)+NULL;", 8, "combination of operand types");
	EidosAssertScriptRaise("(0:2)+NULL;", 5, "combination of operand types");
	EidosAssertScriptRaise("+NULL;", 0, "operand type NULL is not supported");
	EidosAssertScriptSuccess("1+1;", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("1+-1;", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("(0:2)+10;", new EidosValue_Int_vector{10, 11, 12});
	EidosAssertScriptSuccess("10+(0:2);", new EidosValue_Int_vector{10, 11, 12});
	EidosAssertScriptSuccess("(15:13)+(0:2);", new EidosValue_Int_vector{15, 15, 15});
	EidosAssertScriptRaise("(15:12)+(0:2);", 7, "operator requires that either");
	EidosAssertScriptSuccess("1+1.0;", new EidosValue_Float_singleton(2));
	EidosAssertScriptSuccess("1.0+1;", new EidosValue_Float_singleton(2));
	EidosAssertScriptSuccess("1.0+-1.0;", new EidosValue_Float_singleton(0));
	EidosAssertScriptSuccess("(0:2.0)+10;", new EidosValue_Float_vector{10, 11, 12});
	EidosAssertScriptSuccess("10.0+(0:2);", new EidosValue_Float_vector{10, 11, 12});
	EidosAssertScriptSuccess("(15.0:13)+(0:2.0);", new EidosValue_Float_vector{15, 15, 15});
	EidosAssertScriptRaise("(15:12.0)+(0:2);", 9, "operator requires that either");
	EidosAssertScriptSuccess("'foo'+5;", new EidosValue_String_singleton("foo5"));
	EidosAssertScriptSuccess("'foo'+5.0;", new EidosValue_String_singleton("foo5"));
	EidosAssertScriptSuccess("'foo'+5.1;", new EidosValue_String_singleton("foo5.1"));
	EidosAssertScriptSuccess("5+'foo';", new EidosValue_String_singleton("5foo"));
	EidosAssertScriptSuccess("5.0+'foo';", new EidosValue_String_singleton("5foo"));
	EidosAssertScriptSuccess("5.1+'foo';", new EidosValue_String_singleton("5.1foo"));
	EidosAssertScriptSuccess("'foo'+1:3;", new EidosValue_String_vector{"foo1", "foo2", "foo3"});
	EidosAssertScriptSuccess("1:3+'foo';", new EidosValue_String_vector{"1foo", "2foo", "3foo"});
	EidosAssertScriptSuccess("'foo'+'bar';", new EidosValue_String_singleton("foobar"));
	EidosAssertScriptSuccess("'foo'+c('bar', 'baz');", new EidosValue_String_vector{"foobar", "foobaz"});
	EidosAssertScriptSuccess("c('bar', 'baz')+'foo';", new EidosValue_String_vector{"barfoo", "bazfoo"});
	EidosAssertScriptSuccess("c('bar', 'baz')+T;", new EidosValue_String_vector{"barT", "bazT"});
	EidosAssertScriptSuccess("F+c('bar', 'baz');", new EidosValue_String_vector{"Fbar", "Fbaz"});
	EidosAssertScriptRaise("T+F;", 1, "combination of operand types");
	EidosAssertScriptRaise("T+T;", 1, "combination of operand types");
	EidosAssertScriptRaise("F+F;", 1, "combination of operand types");
	EidosAssertScriptSuccess("+5;", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("+5.0;", new EidosValue_Float_singleton(5));
	EidosAssertScriptRaise("+'foo';", 0, "is not supported by");
	EidosAssertScriptRaise("+T;", 0, "is not supported by");
	EidosAssertScriptSuccess("3+4+5;", new EidosValue_Int_singleton(12));
	
	// operator +: raise on integer addition overflow for all code paths
	EidosAssertScriptSuccess("5e18;", new EidosValue_Int_singleton(5000000000000000000LL));
	EidosAssertScriptRaise("1e19;", 0, "could not be represented");
	EidosAssertScriptRaise("5e18 + 5e18;", 5, "overflow with the binary");
	EidosAssertScriptRaise("5e18 + c(0, 0, 5e18, 0);", 5, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) + 5e18;", 17, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) + c(0, 0, 5e18, 0);", 17, "overflow with the binary");
	
	// operator -
	#pragma mark operator −
	EidosAssertScriptRaise("NULL-T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T-NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0-NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5-NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'-NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)-NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)-NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("-NULL;", 0, "is not supported by");
	EidosAssertScriptSuccess("1-1;", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("1--1;", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("(0:2)-10;", new EidosValue_Int_vector{-10, -9, -8});
	EidosAssertScriptSuccess("10-(0:2);", new EidosValue_Int_vector{10, 9, 8});
	EidosAssertScriptSuccess("(15:13)-(0:2);", new EidosValue_Int_vector{15, 13, 11});
	EidosAssertScriptRaise("(15:12)-(0:2);", 7, "operator requires that either");
	EidosAssertScriptSuccess("1-1.0;", new EidosValue_Float_singleton(0));
	EidosAssertScriptSuccess("1.0-1;", new EidosValue_Float_singleton(0));
	EidosAssertScriptSuccess("1.0--1.0;", new EidosValue_Float_singleton(2));
	EidosAssertScriptSuccess("(0:2.0)-10;", new EidosValue_Float_vector{-10, -9, -8});
	EidosAssertScriptSuccess("10.0-(0:2);", new EidosValue_Float_vector{10, 9, 8});
	EidosAssertScriptSuccess("(15.0:13)-(0:2.0);", new EidosValue_Float_vector{15, 13, 11});
	EidosAssertScriptRaise("(15:12.0)-(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'-1;", 5, "is not supported by");
	EidosAssertScriptRaise("T-F;", 1, "is not supported by");
	EidosAssertScriptRaise("T-T;", 1, "is not supported by");
	EidosAssertScriptRaise("F-F;", 1, "is not supported by");
	EidosAssertScriptSuccess("-5;", new EidosValue_Int_singleton(-5));
	EidosAssertScriptSuccess("-5.0;", new EidosValue_Float_singleton(-5));
	EidosAssertScriptRaise("-'foo';", 0, "is not supported by");
	EidosAssertScriptRaise("-T;", 0, "is not supported by");
	EidosAssertScriptSuccess("3-4-5;", new EidosValue_Int_singleton(-6));
	
	// operator -: raise on integer subtraction overflow for all code paths
	EidosAssertScriptSuccess("9223372036854775807;", new EidosValue_Int_singleton(INT64_MAX));
	EidosAssertScriptSuccess("-9223372036854775807 - 1;", new EidosValue_Int_singleton(INT64_MIN));
	EidosAssertScriptRaise("-(-9223372036854775807 - 1);", 0, "overflow with the unary");
	EidosAssertScriptSuccess("-5e18;", new EidosValue_Int_singleton(-5000000000000000000LL));
	EidosAssertScriptRaise("-5e18 - 5e18;", 6, "overflow with the binary");
	EidosAssertScriptRaise("-5e18 - c(0, 0, 5e18, 0);", 6, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, -5e18, 0) - 5e18;", 18, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, -5e18, 0) - c(0, 0, 5e18, 0);", 18, "overflow with the binary");
	
    // operator *
	#pragma mark operator *
	EidosAssertScriptRaise("NULL*T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T*NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0*NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5*NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'*NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)*NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)*NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("*NULL;", 0, "unexpected token");
    EidosAssertScriptSuccess("1*1;", new EidosValue_Int_singleton(1));
    EidosAssertScriptSuccess("1*-1;", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("(0:2)*10;", new EidosValue_Int_vector{0, 10, 20});
	EidosAssertScriptSuccess("10*(0:2);", new EidosValue_Int_vector{0, 10, 20});
	EidosAssertScriptSuccess("(15:13)*(0:2);", new EidosValue_Int_vector{0, 14, 26});
	EidosAssertScriptRaise("(15:12)*(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess("1*1.0;", new EidosValue_Float_singleton(1));
    EidosAssertScriptSuccess("1.0*1;", new EidosValue_Float_singleton(1));
    EidosAssertScriptSuccess("1.0*-1.0;", new EidosValue_Float_singleton(-1));
	EidosAssertScriptSuccess("(0:2.0)*10;", new EidosValue_Float_vector{0, 10, 20});
	EidosAssertScriptSuccess("10.0*(0:2);", new EidosValue_Float_vector{0, 10, 20});
	EidosAssertScriptSuccess("(15.0:13)*(0:2.0);", new EidosValue_Float_vector{0, 14, 26});
	EidosAssertScriptRaise("(15:12.0)*(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'*5;", 5, "is not supported by");
	EidosAssertScriptRaise("T*F;", 1, "is not supported by");
	EidosAssertScriptRaise("T*T;", 1, "is not supported by");
	EidosAssertScriptRaise("F*F;", 1, "is not supported by");
	EidosAssertScriptRaise("*5;", 0, "unexpected token");
	EidosAssertScriptRaise("*5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("*'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("*T;", 0, "unexpected token");
    EidosAssertScriptSuccess("3*4*5;", new EidosValue_Int_singleton(60));
	
	// operator *: raise on integer multiplication overflow for all code paths
	EidosAssertScriptSuccess("5e18;", new EidosValue_Int_singleton(5000000000000000000LL));
	EidosAssertScriptRaise("1e19;", 0, "could not be represented");
	EidosAssertScriptRaise("5e18 * 2;", 5, "multiplication overflow");
	EidosAssertScriptRaise("5e18 * c(0, 0, 2, 0);", 5, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 2, 0) * 5e18;", 14, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 2, 0) * c(0, 0, 5e18, 0);", 14, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) * c(0, 0, 2, 0);", 17, "multiplication overflow");
	
    // operator /
	#pragma mark operator /
	EidosAssertScriptRaise("NULL/T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T/NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0/NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5/NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'/NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)/NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)/NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("/NULL;", 0, "unexpected token");
    EidosAssertScriptSuccess("1/1;", new EidosValue_Float_singleton(1));
    EidosAssertScriptSuccess("1/-1;", new EidosValue_Float_singleton(-1));
	EidosAssertScriptSuccess("(0:2)/10;", new EidosValue_Float_vector{0, 0.1, 0.2});
	EidosAssertScriptRaise("(15:12)/(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess("1/1.0;", new EidosValue_Float_singleton(1));
    EidosAssertScriptSuccess("1.0/1;", new EidosValue_Float_singleton(1));
    EidosAssertScriptSuccess("1.0/-1.0;", new EidosValue_Float_singleton(-1));
	EidosAssertScriptSuccess("(0:2.0)/10;", new EidosValue_Float_vector{0, 0.1, 0.2});
	EidosAssertScriptSuccess("10.0/(0:2);", new EidosValue_Float_vector{std::numeric_limits<double>::infinity(), 10, 5});
	EidosAssertScriptSuccess("(15.0:13)/(0:2.0);", new EidosValue_Float_vector{std::numeric_limits<double>::infinity(), 14, 6.5});
	EidosAssertScriptRaise("(15:12.0)/(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'/5;", 5, "is not supported by");
	EidosAssertScriptRaise("T/F;", 1, "is not supported by");
	EidosAssertScriptRaise("T/T;", 1, "is not supported by");
	EidosAssertScriptRaise("F/F;", 1, "is not supported by");
	EidosAssertScriptRaise("/5;", 0, "unexpected token");
	EidosAssertScriptRaise("/5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("/'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("/T;", 0, "unexpected token");
    EidosAssertScriptSuccess("3/4/5;", new EidosValue_Float_singleton(0.15));
	EidosAssertScriptSuccess("6/0;", new EidosValue_Float_singleton(std::numeric_limits<double>::infinity()));
    
    // operator %
	#pragma mark operator %
	EidosAssertScriptRaise("NULL%T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T%NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0%NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5%NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'%NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)%NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)%NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("%NULL;", 0, "unexpected token");
    EidosAssertScriptSuccess("1%1;", new EidosValue_Float_singleton(0));
    EidosAssertScriptSuccess("1%-1;", new EidosValue_Float_singleton(0));
	EidosAssertScriptSuccess("(0:2)%10;", new EidosValue_Float_vector{0, 1, 2});
	EidosAssertScriptRaise("(15:12)%(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess("1%1.0;", new EidosValue_Float_singleton(0));
    EidosAssertScriptSuccess("1.0%1;", new EidosValue_Float_singleton(0));
    EidosAssertScriptSuccess("1.0%-1.0;", new EidosValue_Float_singleton(0));
	EidosAssertScriptSuccess("(0:2.0)%10;", new EidosValue_Float_vector{0, 1, 2});
	EidosAssertScriptSuccess("10.0%(0:4);", new EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN(), 0, 0, 1, 2});
	EidosAssertScriptSuccess("(15.0:13)%(0:2.0);", new EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN(), 0, 1});
	EidosAssertScriptRaise("(15:12.0)%(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'%5;", 5, "is not supported by");
	EidosAssertScriptRaise("T%F;", 1, "is not supported by");
	EidosAssertScriptRaise("T%T;", 1, "is not supported by");
	EidosAssertScriptRaise("F%F;", 1, "is not supported by");
	EidosAssertScriptRaise("%5;", 0, "unexpected token");
	EidosAssertScriptRaise("%5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("%'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("%T;", 0, "unexpected token");
    EidosAssertScriptSuccess("3%4%5;", new EidosValue_Float_singleton(3));

	// operator []
	#pragma mark operator []
	EidosAssertScriptRaise("x = 1:5; x[NULL];", 10, "is not supported by");
	EidosAssertScriptSuccess("x = 1:5; NULL[x];", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; NULL[NULL];", gStaticEidosValueNULL);
	EidosAssertScriptRaise("x = 1:5; x[];", 11, "unexpected token");
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)];", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("x = 1:5; x[2];", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("x = 1:5; x[2:3];", new EidosValue_Int_vector{3, 4});
	EidosAssertScriptSuccess("x = 1:5; x[c(0, 2, 4)];", new EidosValue_Int_vector{1, 3, 5});
	EidosAssertScriptSuccess("x = 1:5; x[0:4];", new EidosValue_Int_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("x = 1:5; x[float(0)];", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("x = 1:5; x[2.0];", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("x = 1:5; x[2.0:3];", new EidosValue_Int_vector{3, 4});
	EidosAssertScriptSuccess("x = 1:5; x[c(0.0, 2, 4)];", new EidosValue_Int_vector{1, 3, 5});
	EidosAssertScriptSuccess("x = 1:5; x[0.0:4];", new EidosValue_Int_vector{1, 2, 3, 4, 5});
	EidosAssertScriptRaise("x = 1:5; x[logical(0)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[T];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, T)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, F, T)];", 10, "operator requires that the size()");
	EidosAssertScriptSuccess("x = 1:5; x[c(T, F, T, F, T)];", new EidosValue_Int_vector{1, 3, 5});
	EidosAssertScriptSuccess("x = 1:5; x[c(T, T, T, T, T)];", new EidosValue_Int_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("x = 1:5; x[c(F, F, F, F, F)];", new EidosValue_Int_vector());
	
	// operator = (especially in conjunction with operator [])
	#pragma mark operator = with []
	EidosAssertScriptSuccess("x = 5; x;", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("x = 1:5; x;", new EidosValue_Int_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 10; x;", new EidosValue_Int_vector{10, 2, 10, 4, 10});
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 10; x;", new EidosValue_Int_vector{1, 2, 10, 4, 10});
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 10; x;", new EidosValue_Int_vector{10, 2, 10, 4, 10});
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 10; x;", new EidosValue_Int_vector{10, 2, 10, 4, 5});
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 11:13; x;", new EidosValue_Int_vector{11, 2, 12, 4, 13});
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 11:12; x;", new EidosValue_Int_vector{1, 2, 11, 4, 12});
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 11:13; x;", new EidosValue_Int_vector{11, 2, 12, 4, 13});
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 11:12; x;", new EidosValue_Int_vector{11, 2, 12, 4, 5});
	EidosAssertScriptRaise("x = 1:5; x[1:3*2 - 2][0:1] = 11:13; x;", 27, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[NULL] = NULL; x;", 10, "is not supported by");
	EidosAssertScriptRaise("x = 1:5; x[NULL] = 10; x;", 10, "is not supported by");
	EidosAssertScriptRaise("x = 1:5; x[3] = NULL; x;", 14, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[integer(0)] = NULL; x;", 23, "type mismatch");
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)] = 10; x;", new EidosValue_Int_vector{1, 2, 3, 4, 5}); // assigns 10 to no indices, perfectly legal
	EidosAssertScriptRaise("x = 1:5; x[3] = integer(0); x;", 14, "assignment to a subscript requires");
	EidosAssertScriptSuccess("x = 1.0:5; x[3] = 1; x;", new EidosValue_Float_vector{1, 2, 3, 1, 5});
	EidosAssertScriptSuccess("x = c('a', 'b', 'c'); x[1] = 1; x;", new EidosValue_String_vector{"a", "1", "c"});
	EidosAssertScriptRaise("x = 1:5; x[3] = 1.5; x;", 14, "type mismatch");
	EidosAssertScriptRaise("x = 1:5; x[3] = 'foo'; x;", 14, "type mismatch");
	EidosAssertScriptSuccess("x = 5; x[0] = 10; x;", new EidosValue_Int_singleton(10));
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10.0; x;", new EidosValue_Float_singleton(10));
	EidosAssertScriptRaise("x = 5; x[0] = 10.0; x;", 12, "type mismatch");
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10; x;", new EidosValue_Float_singleton(10));
	EidosAssertScriptSuccess("x = T; x[0] = F; x;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("x = 'foo'; x[0] = 'bar'; x;", new EidosValue_String_singleton("bar"));
	
	// operator = (especially in conjunction with operator .)
	#pragma mark operator = with .
	EidosAssertScriptSuccess("x=_Test(9); x._yolk;", new EidosValue_Int_singleton(9));
	EidosAssertScriptRaise("x=_Test(NULL);", 2, "cannot be type NULL");
	EidosAssertScriptRaise("x=_Test(9); x._yolk = NULL;", 20, "assignment to a property requires");
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk;", new EidosValue_Int_vector{9, 7, 9, 7});
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=2; z._yolk;", new EidosValue_Int_vector{9, 2, 9, 2});
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=2; z._yolk;", new EidosValue_Int_vector{9, 2, 9, 2});
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[c(1,0)]._yolk=c(2, 5); z._yolk;", new EidosValue_Int_vector{5, 2, 5, 2});
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[c(1,0)]=c(3, 6); z._yolk;", new EidosValue_Int_vector{6, 3, 6, 3});
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=6.5; z._yolk;", 48, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=6.5; z._yolk;", 48, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2:3]._yolk=6.5; z._yolk;", 50, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[2:3]=6.5; z._yolk;", 50, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2]=6.5; z._yolk;", 42, "type mismatch");
	
	// operator >
	#pragma mark operator >
	EidosAssertScriptRaise("NULL>T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>'foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL>(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T>NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0>NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5>NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'>NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)>NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)>NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise(">NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T > F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > -5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5 > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > -5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5.0 > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 > T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > 'FOO';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'FOO' > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 'XYZZY';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'XYZZY' > T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 > -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 > -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 > 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' > 'bar';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'bar' > 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 > '10';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("10 > '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 > '15';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("15 > '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) > 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 > _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 > -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > '5';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'5' > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' > 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) > _Test(9);", 9, "cannot be used with type");
	
	// operator <
	#pragma mark operator <
	EidosAssertScriptRaise("NULL<T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<'foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL<(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T<NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0<NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5<NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'<NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)<NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)<NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("<NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T < F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F < F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F < 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F < 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < -5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5 < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < -5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5.0 < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < 'FOO';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'FOO' < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < 'XYZZY';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'XYZZY' < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 < -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 < 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 < -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 < 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 < -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 < 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' < 'bar';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'bar' < 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 < '10';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("10 < '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 < '15';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("15 < '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) < 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 < _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 < 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 < -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 < 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 < 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 < '5';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'5' < 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' < 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) < _Test(9);", 9, "cannot be used with type");
	
	// operator >=
	#pragma mark operator >=
	EidosAssertScriptRaise("NULL>=T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>=0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>=0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>=_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL>=(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T>=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0>=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5>=NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'>=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)>=NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)>=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise(">=NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T >= F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F >= F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F >= 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F >= 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= -5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5 >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= -5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5.0 >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= 'FOO';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'FOO' >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= 'XYZZY';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'XYZZY' >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 >= -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 >= 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 >= -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 >= 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 >= -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 >= 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' >= 'bar';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'bar' >= 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 >= '10';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("10 >= '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 >= '15';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("15 >= '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) >= 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 >= _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 >= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 >= -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 >= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 >= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 >= '5';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'5' >= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' >= 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) >= _Test(9);", 9, "cannot be used with type");
	
	// operator <=
	#pragma mark operator <=
	EidosAssertScriptRaise("NULL<=T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<=0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<=0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<=_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL<=(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T<=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0<=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5<=NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'<=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)<=NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)<=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("<=NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T <= F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= -5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5 <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= -5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5.0 <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 <= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= 'FOO';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'FOO' <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 'XYZZY';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'XYZZY' <= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 <= -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 <= -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 <= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' <= 'bar';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'bar' <= 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 <= '10';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("10 <= '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 <= '15';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("15 <= '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) <= 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 <= _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 <= -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= '5';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'5' <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' <= 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) <= _Test(9);", 9, "cannot be used with type");
	
	// operator ==
	#pragma mark operator ==
	EidosAssertScriptRaise("NULL==T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL=='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==_Test(7);", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T==NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0==NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5==NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'==NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)==NULL;", 8, "testing NULL with");
	EidosAssertScriptRaise("(0:2)==NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("==NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T == F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F == F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T == 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F == 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F == 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == -5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == -5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5.0 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 'FOO';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'FOO' == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 'XYZZY';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'XYZZY' == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 == -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 == 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 == -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 == 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 == -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 == 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' == 'bar';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'bar' == 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 == '10';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("10 == '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 == '15';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("15 == '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) == 5;", 9, "cannot be converted to");
	EidosAssertScriptRaise("5 == _Test(9);", 2, "cannot be converted to");
	EidosAssertScriptSuccess("5 == 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 == -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 == 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 == 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 == '5';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'5' == 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' == 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("_Test(9) == _Test(9);", gStaticEidosValue_LogicalF);	// not the same object
	
	// operator !=
	#pragma mark operator !=
	EidosAssertScriptRaise("NULL!=T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=_Test(7);", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T!=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0!=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5!=NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'!=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)!=NULL;", 8, "testing NULL with");
	EidosAssertScriptRaise("(0:2)!=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("!=NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T != F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F != F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T != 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F != 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F != 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != -5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != -5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5.0 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 'FOO';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'FOO' != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 'XYZZY';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'XYZZY' != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 != -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 != 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 != -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 != 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 != -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 != 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' != 'bar';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'bar' != 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 != '10';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("10 != '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 != '15';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("15 != '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) != 5;", 9, "cannot be converted to");
	EidosAssertScriptRaise("5 != _Test(9);", 2, "cannot be converted to");
	EidosAssertScriptSuccess("5 != 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 != -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 != 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 != 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 != '5';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'5' != 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' != 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("_Test(9) != _Test(9);", gStaticEidosValue_LogicalT);	// not the same object
	
	// operator :
	#pragma mark operator :
	EidosAssertScriptRaise("NULL:T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T:NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0:NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5:NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo':NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7):NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2):NULL;", 5, "is not supported by");
	EidosAssertScriptRaise(":NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("1:5;", new EidosValue_Int_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("5:1;", new EidosValue_Int_vector{5, 4, 3, 2, 1});
	EidosAssertScriptSuccess("-2:1;", new EidosValue_Int_vector{-2, -1, 0, 1});
	EidosAssertScriptSuccess("1:-2;", new EidosValue_Int_vector{1, 0, -1, -2});
	EidosAssertScriptSuccess("1:1;", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("1.0:5;", new EidosValue_Float_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("5.0:1;", new EidosValue_Float_vector{5, 4, 3, 2, 1});
	EidosAssertScriptSuccess("-2.0:1;", new EidosValue_Float_vector{-2, -1, 0, 1});
	EidosAssertScriptSuccess("1.0:-2;", new EidosValue_Float_vector{1, 0, -1, -2});
	EidosAssertScriptSuccess("1.0:1;", new EidosValue_Float_singleton(1));
	EidosAssertScriptSuccess("1:5.0;", new EidosValue_Float_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("5:1.0;", new EidosValue_Float_vector{5, 4, 3, 2, 1});
	EidosAssertScriptSuccess("-2:1.0;", new EidosValue_Float_vector{-2, -1, 0, 1});
	EidosAssertScriptSuccess("1:-2.0;", new EidosValue_Float_vector{1, 0, -1, -2});
	EidosAssertScriptSuccess("1:1.0;", new EidosValue_Float_singleton(1));
	EidosAssertScriptRaise("1:F;", 1, "is not supported by");
	EidosAssertScriptRaise("F:1;", 1, "is not supported by");
	EidosAssertScriptRaise("T:F;", 1, "is not supported by");
	EidosAssertScriptRaise("'a':'z';", 3, "is not supported by");
	EidosAssertScriptRaise("1:(2:3);", 1, "operator must have size()");
	EidosAssertScriptRaise("(1:2):3;", 5, "operator must have size()");
	EidosAssertScriptSuccess("1.5:4.7;", new EidosValue_Float_vector{1.5, 2.5, 3.5, 4.5});
	EidosAssertScriptSuccess("1.5:-2.7;", new EidosValue_Float_vector{1.5, 0.5, -0.5, -1.5, -2.5});
	EidosAssertScriptRaise("1.5:INF;", 3, "range with more than");
	EidosAssertScriptRaise("1.5:NAN;", 3, "must not be NAN");
	EidosAssertScriptRaise("INF:1.5;", 3, "range with more than");
	EidosAssertScriptRaise("NAN:1.5;", 3, "must not be NAN");
	
	// operator ^
	#pragma mark operator ^
	EidosAssertScriptRaise("NULL^T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T^NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0^NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5^NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'^NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)^NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)^NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("^NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("1^1;", new EidosValue_Float_singleton(1));
	EidosAssertScriptSuccess("1^-1;", new EidosValue_Float_singleton(1));
	EidosAssertScriptSuccess("(0:2)^10;", new EidosValue_Float_vector{0, 1, 1024});
	EidosAssertScriptSuccess("10^(0:2);", new EidosValue_Float_vector{1, 10, 100});
	EidosAssertScriptSuccess("(15:13)^(0:2);", new EidosValue_Float_vector{1, 14, 169});
	EidosAssertScriptRaise("(15:12)^(0:2);", 7, "operator requires that either");
	EidosAssertScriptRaise("NULL^(0:2);", 4, "is not supported by");
	EidosAssertScriptSuccess("1^1.0;", new EidosValue_Float_singleton(1));
	EidosAssertScriptSuccess("1.0^1;", new EidosValue_Float_singleton(1));
	EidosAssertScriptSuccess("1.0^-1.0;", new EidosValue_Float_singleton(1));
	EidosAssertScriptSuccess("(0:2.0)^10;", new EidosValue_Float_vector{0, 1, 1024});
	EidosAssertScriptSuccess("10.0^(0:2);", new EidosValue_Float_vector{1, 10, 100});
	EidosAssertScriptSuccess("(15.0:13)^(0:2.0);", new EidosValue_Float_vector{1, 14, 169});
	EidosAssertScriptRaise("(15:12.0)^(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("NULL^(0:2.0);", 4, "is not supported by");
	EidosAssertScriptRaise("'foo'^5;", 5, "is not supported by");
	EidosAssertScriptRaise("T^F;", 1, "is not supported by");
	EidosAssertScriptRaise("T^T;", 1, "is not supported by");
	EidosAssertScriptRaise("F^F;", 1, "is not supported by");
	EidosAssertScriptRaise("^5;", 0, "unexpected token");
	EidosAssertScriptRaise("^5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("^'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("^T;", 0, "unexpected token");
	EidosAssertScriptSuccess("4^(3^2);", new EidosValue_Float_singleton(262144));		// right-associative!
	EidosAssertScriptSuccess("4^3^2;", new EidosValue_Float_singleton(262144));		// right-associative!
	
	// operator &
	#pragma mark operator &
	EidosAssertScriptRaise("NULL&T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T&NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0&NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5&NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'&NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)&NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)&NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("&NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&T&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&F&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & F;", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("c(T,F,T,F) & T;", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("F & c(T,F,T,F);", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("T & c(T,F,T,F);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c(T,F,T,F) & c(T,T,F,F);", new EidosValue_Logical{true, false, false, false});
	EidosAssertScriptSuccess("c(T,F,T,F) & c(F,F,T,T);", new EidosValue_Logical{false, false, true, false});
	EidosAssertScriptSuccess("c(T,T,F,F) & c(T,F,T,F);", new EidosValue_Logical{true, false, false, false});
	EidosAssertScriptSuccess("c(F,F,T,T) & c(T,F,T,F);", new EidosValue_Logical{false, false, true, false});
	EidosAssertScriptRaise("c(T,F,T,F) & c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) & c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) & _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) & c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess("5&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&5&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("0&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & 0;", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("c(7,0,5,0) & T;", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("F & c(5,0,7,0);", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("9 & c(T,F,T,F);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c(7,0,5,0) & c(T,T,F,F);", new EidosValue_Logical{true, false, false, false});
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0,0,5,7);", new EidosValue_Logical{false, false, true, false});
	EidosAssertScriptSuccess("5.0&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&5.0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("0.0&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&0.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0.0&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0.0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & 0.0;", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & T;", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("F & c(5.0,0.0,7.0,0.0);", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("9.0 & c(T,F,T,F);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & c(T,T,F,F);", new EidosValue_Logical{true, false, false, false});
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0.0,0.0,5.0,7.0);", new EidosValue_Logical{false, false, true, false});
	EidosAssertScriptSuccess("INF&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&INF&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("T&NAN&F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN&T&T;", 3, "cannot be converted");
	EidosAssertScriptSuccess("'foo'&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&'foo'&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo'&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("''&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&'';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&''&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&''&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & '';", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("c('foo','','foo','') & T;", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("F & c('foo','','foo','');", new EidosValue_Logical{false, false, false, false});
	EidosAssertScriptSuccess("'foo' & c(T,F,T,F);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c('foo','','foo','') & c(T,T,F,F);", new EidosValue_Logical{true, false, false, false});
	EidosAssertScriptSuccess("c(T,F,T,F) & c('','','foo','foo');", new EidosValue_Logical{false, false, true, false});
	
	// operator |
	#pragma mark operator |
	EidosAssertScriptRaise("NULL|T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T|NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0|NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5|NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'|NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)|NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)|NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("|NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|T|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|F|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|F|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | F;", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c(T,F,T,F) | T;", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("F | c(T,F,T,F);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("T | c(T,F,T,F);", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("c(T,F,T,F) | c(T,T,F,F);", new EidosValue_Logical{true, true, true, false});
	EidosAssertScriptSuccess("c(T,F,T,F) | c(F,F,T,T);", new EidosValue_Logical{true, false, true, true});
	EidosAssertScriptSuccess("c(T,T,F,F) | c(T,F,T,F);", new EidosValue_Logical{true, true, true, false});
	EidosAssertScriptSuccess("c(F,F,T,T) | c(T,F,T,F);", new EidosValue_Logical{true, false, true, true});
	EidosAssertScriptRaise("c(T,F,T,F) | c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) | c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) | _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) | c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess("5|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|5|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | 0;", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c(7,0,5,0) | T;", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("F | c(5,0,7,0);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("9 | c(T,F,T,F);", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("c(7,0,5,0) | c(T,T,F,F);", new EidosValue_Logical{true, true, true, false});
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0,0,5,7);", new EidosValue_Logical{true, false, true, true});
	EidosAssertScriptSuccess("5.0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|5.0|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("0.0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0.0|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0.0|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | 0.0;", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | T;", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("F | c(5.0,0.0,7.0,0.0);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("9.0 | c(T,F,T,F);", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | c(T,T,F,F);", new EidosValue_Logical{true, true, true, false});
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0.0,0.0,5.0,7.0);", new EidosValue_Logical{true, false, true, true});
	EidosAssertScriptSuccess("INF|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|INF|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("T|NAN|F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN|T|T;", 3, "cannot be converted");
	EidosAssertScriptSuccess("'foo'|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|'foo'|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo'|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("''|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|'';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|''|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|''|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | '';", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("c('foo','','foo','') | T;", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("F | c('foo','','foo','');", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("'foo' | c(T,F,T,F);", new EidosValue_Logical{true, true, true, true});
	EidosAssertScriptSuccess("c('foo','','foo','') | c(T,T,F,F);", new EidosValue_Logical{true, true, true, false});
	EidosAssertScriptSuccess("c(T,F,T,F) | c('','','foo','foo');", new EidosValue_Logical{true, false, true, true});
	
	// operator !
	#pragma mark operator !
	EidosAssertScriptRaise("!NULL;", 0, "is not supported by");
	EidosAssertScriptSuccess("!T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!c(F,T,F,T);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("!c(0,5,0,1);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("!c(0,5.0,0,1.0);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptRaise("!c(0,NAN,0,1.0);", 0, "cannot be converted");
	EidosAssertScriptSuccess("!c(0,INF,0,1.0);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("!c('','foo','','bar');", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptRaise("!_Test(5);", 0, "is not supported by");
	
	
	// ************************************************************************************
	//
	//	Keyword tests
	//
	
	// if
	#pragma mark if
	EidosAssertScriptSuccess("if (T) 23;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if (F) 23;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (6 > 5) 23;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if (6 < 5) 23;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (6 == (6:9)) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (_Test(6)) 23;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23;", 0, "condition for if statement has size()");
	
	// if-else
	#pragma mark if-else
	EidosAssertScriptSuccess("if (T) 23; else 42;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if (F) 23; else 42;", new EidosValue_Int_singleton(42));
	EidosAssertScriptSuccess("if (6 > 5) 23; else 42;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if (6 < 5) 23; else 42;", new EidosValue_Int_singleton(42));
	EidosAssertScriptRaise("if (6 == (6:9)) 23; else 42;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23; else 42;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23; else 42;", new EidosValue_Int_singleton(42));
	EidosAssertScriptRaise("if (_Test(6)) 23; else 42;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23; else 42;", 0, "condition for if statement has size()");
	
	// do
	#pragma mark do
	EidosAssertScriptSuccess("x=1; do x=x*2; while (x<100); x;", new EidosValue_Int_singleton(128));
	EidosAssertScriptSuccess("x=200; do x=x*2; while (x<100); x;", new EidosValue_Int_singleton(400));
	EidosAssertScriptSuccess("x=1; do { x=x*2; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton(127));
	EidosAssertScriptSuccess("x=200; do { x=x*2; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton(401));
	EidosAssertScriptRaise("x=1; do x=x*2; while (x < 100:102); x;", 5, "condition for do-while loop has size()");
	EidosAssertScriptRaise("x=200; do x=x*2; while (x < 100:102); x;", 7, "condition for do-while loop has size()");
	EidosAssertScriptSuccess("x=1; do x=x*2; while ((x < 100:102)[0]); x;", new EidosValue_Int_singleton(128));
	EidosAssertScriptSuccess("x=200; do x=x*2; while ((x < 100:102)[0]); x;", new EidosValue_Int_singleton(400));
	EidosAssertScriptRaise("x=200; do x=x*2; while (_Test(6)); x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; do x=x*2; while (NULL); x;", 7, "condition for do-while loop has size()");
	
	// while
	#pragma mark while
	EidosAssertScriptSuccess("x=1; while (x<100) x=x*2; x;", new EidosValue_Int_singleton(128));
	EidosAssertScriptSuccess("x=200; while (x<100) x=x*2; x;", new EidosValue_Int_singleton(200));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; x=x+1; } x;", new EidosValue_Int_singleton(127));
	EidosAssertScriptSuccess("x=200; while (x<100) { x=x*2; x=x+1; } x;", new EidosValue_Int_singleton(200));
	EidosAssertScriptRaise("x=1; while (x < 100:102) x=x*2; x;", 5, "condition for while loop has size()");
	EidosAssertScriptRaise("x=200; while (x < 100:102) x=x*2; x;", 7, "condition for while loop has size()");
	EidosAssertScriptSuccess("x=1; while ((x < 100:102)[0]) x=x*2; x;", new EidosValue_Int_singleton(128));
	EidosAssertScriptSuccess("x=200; while ((x < 100:102)[0]) x=x*2; x;", new EidosValue_Int_singleton(200));
	EidosAssertScriptRaise("x=200; while (_Test(6)) x=x*2; x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; while (NULL) x=x*2; x;", 7, "condition for while loop has size()");
	
	// for and in
	#pragma mark for / in
	EidosAssertScriptSuccess("x=0; for (y in 33) x=x+y; x;", new EidosValue_Int_singleton(33));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) x=x+y; x;", new EidosValue_Int_singleton(55));
	EidosAssertScriptSuccess("x=0; for (y in 10:1) x=x+y; x;", new EidosValue_Int_singleton(55));
	EidosAssertScriptSuccess("x=0; for (y in 1.0:10) x=x+y; x;", new EidosValue_Float_singleton(55.0));
	EidosAssertScriptSuccess("x=0; for (y in c('foo', 'bar')) x=x+y; x;", new EidosValue_String_singleton("0foobar"));
	EidosAssertScriptSuccess("x=0; for (y in c(T,T,F,F,T,F)) x=x+asInteger(y); x;", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("x=0; for (y in _Test(7)) x=x+y._yolk; x;", new EidosValue_Int_singleton(7));
	EidosAssertScriptSuccess("x=0; for (y in rep(_Test(7),3)) x=x+y._yolk; x;", new EidosValue_Int_singleton(21));
	EidosAssertScriptRaise("x=0; y=0:2; for (y[0] in 2:4) x=x+sum(y); x;", 18, "unexpected token");	// lvalue must be an identifier, at present
	EidosAssertScriptRaise("x=0; for (y in NULL) x;", 5, "does not allow NULL");
	
	// next
	#pragma mark next
	EidosAssertScriptRaise("next;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) next;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) next;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (T) next; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) next; else 42;", new EidosValue_Int_singleton(42));
	EidosAssertScriptSuccess("if (T) 23; else next;", new EidosValue_Int_singleton(23));
	EidosAssertScriptRaise("if (F) 23; else next;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) next; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton(124));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) next; x=x+1; } x;", new EidosValue_Int_singleton(124));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) next; x=x+y; } x;", new EidosValue_Int_singleton(50));
	
	// break
	#pragma mark break
	EidosAssertScriptRaise("break;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) break;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) break;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (T) break; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) break; else 42;", new EidosValue_Int_singleton(42));
	EidosAssertScriptSuccess("if (T) 23; else break;", new EidosValue_Int_singleton(23));
	EidosAssertScriptRaise("if (F) 23; else break;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) break; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton(62));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) break; x=x+1; } x;", new EidosValue_Int_singleton(62));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) break; x=x+y; } x;", new EidosValue_Int_singleton(10));
	
	// return
	#pragma mark return
	EidosAssertScriptSuccess("return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("return -13;", new EidosValue_Int_singleton(-13));
	EidosAssertScriptSuccess("if (T) return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13;", new EidosValue_Int_singleton(-13));
	EidosAssertScriptSuccess("if (F) return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) return -13;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return; else 42;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13; else 42;", new EidosValue_Int_singleton(-13));
	EidosAssertScriptSuccess("if (F) return; else 42;", new EidosValue_Int_singleton(42));
	EidosAssertScriptSuccess("if (F) return -13; else 42;", new EidosValue_Int_singleton(42));
	EidosAssertScriptSuccess("if (T) 23; else return;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if (T) 23; else return -13;", new EidosValue_Int_singleton(23));
	EidosAssertScriptSuccess("if (F) 23; else return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) 23; else return -13;", new EidosValue_Int_singleton(-13));
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return; x=x+1; } while (x<100); x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return x-5; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton(57));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return; x=x+1; } x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return x-5; x=x+1; } x;", new EidosValue_Int_singleton(57));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return; x=x+y; } x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return x-5; x=x+y; } x;", new EidosValue_Int_singleton(5));
	
	
	// ************************************************************************************
	//
	//	Function tests
	//
	#pragma mark -
	#pragma mark Functions
	#pragma mark -
	
	#pragma mark math
	
	// abs()
	EidosAssertScriptSuccess("abs(5);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("abs(-5);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("abs(c(-2, 7, -18, 12));", new EidosValue_Int_vector{2, 7, 18, 12});
	EidosAssertScriptSuccess("abs(5.5);", new EidosValue_Float_singleton(5.5));
	EidosAssertScriptSuccess("abs(-5.5);", new EidosValue_Float_singleton(5.5));
	EidosAssertScriptSuccess("abs(c(-2.0, 7.0, -18.0, 12.0));", new EidosValue_Float_vector{2, 7, 18, 12});
	EidosAssertScriptRaise("abs(T);", 0, "cannot be type");
	EidosAssertScriptRaise("abs('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("abs(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("abs(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("abs(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("abs(integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("abs(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("abs(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("-9223372036854775807 - 1;", new EidosValue_Int_singleton(INT64_MIN));
	EidosAssertScriptRaise("abs(-9223372036854775807 - 1);", 0, "most negative integer");
	
	// acos()
	EidosAssertScriptSuccess("abs(acos(0) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(acos(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(acos(c(0, 1, -1)) - c(PI/2, 0, PI))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(acos(0.0) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(acos(1.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(acos(c(0.0, 1.0, -1.0)) - c(PI/2, 0, PI))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("acos(T);", 0, "cannot be type");
	EidosAssertScriptRaise("acos('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("acos(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("acos(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("acos(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("acos(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("acos(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("acos(string(0));", 0, "cannot be type");
	
	// asin()
	EidosAssertScriptSuccess("abs(asin(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(asin(1) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(asin(c(0, 1, -1)) - c(0, PI/2, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(asin(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(asin(1.0) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(asin(c(0.0, 1.0, -1.0)) - c(0, PI/2, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("asin(T);", 0, "cannot be type");
	EidosAssertScriptRaise("asin('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("asin(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("asin(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("asin(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("asin(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("asin(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("asin(string(0));", 0, "cannot be type");
	
	// atan()
	EidosAssertScriptSuccess("abs(atan(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan(1) - PI/4) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan(c(0, 1, -1)) - c(0, PI/4, -PI/4))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan(1.0) - PI/4) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan(c(0.0, 1.0, -1.0)) - c(0, PI/4, -PI/4))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("atan(T);", 0, "cannot be type");
	EidosAssertScriptRaise("atan('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("atan(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("atan(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("atan(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("atan(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("atan(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("atan(string(0));", 0, "cannot be type");
	
	// atan2()
	EidosAssertScriptSuccess("abs(atan2(0, 1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0, -1) - PI) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan2(c(0, 0, -1), c(1, -1, 0)) - c(0, PI, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0.0, 1.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0.0, -1.0) - PI) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan2(c(0.0, 0.0, -1.0), c(1.0, -1.0, 0.0)) - c(0, PI, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("atan2(T);", 0, "cannot be type");
	EidosAssertScriptRaise("atan2('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(logical(0), logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("atan2(integer(0), integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("atan2(float(0), float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("atan2(string(0), string(0));", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(0.0, c(0.0, 1.0));", 0, "requires arguments of equal length");		// argument count mismatch
	
	// ceil()
	EidosAssertScriptSuccess("ceil(5.1);", new EidosValue_Float_singleton(6.0));
	EidosAssertScriptSuccess("ceil(-5.1);", new EidosValue_Float_singleton(-5.0));
	EidosAssertScriptSuccess("ceil(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector{-2.0, 8, -18, 13});
	EidosAssertScriptRaise("ceil(T);", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(5);", 0, "cannot be type");
	EidosAssertScriptRaise("ceil('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("ceil(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("ceil(string(0));", 0, "cannot be type");
	
	// cos()
	EidosAssertScriptSuccess("abs(cos(0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cos(0.0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cos(PI/2) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(cos(c(0, PI/2, PI)) - c(1, 0, -1))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("cos(T);", 0, "cannot be type");
	EidosAssertScriptRaise("cos('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cos(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cos(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cos(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("cos(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("cos(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("cos(string(0));", 0, "cannot be type");
	
	// exp()
	EidosAssertScriptSuccess("abs(exp(0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(exp(0.0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(exp(1.0) - E) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(exp(c(0, 1.0, -1)) - c(1, E, 0.3678794))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("exp(T);", 0, "cannot be type");
	EidosAssertScriptRaise("exp('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("exp(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("exp(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("exp(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("exp(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("exp(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("exp(string(0));", 0, "cannot be type");
	
	// floor()
	EidosAssertScriptSuccess("floor(5.1);", new EidosValue_Float_singleton(5.0));
	EidosAssertScriptSuccess("floor(-5.1);", new EidosValue_Float_singleton(-6.0));
	EidosAssertScriptSuccess("floor(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector{-3.0, 7, -19, 12});
	EidosAssertScriptRaise("floor(T);", 0, "cannot be type");
	EidosAssertScriptRaise("floor(5);", 0, "cannot be type");
	EidosAssertScriptRaise("floor('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("floor(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("floor(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("floor(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("floor(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("floor(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("floor(string(0));", 0, "cannot be type");
	
	// integerDiv()
	EidosAssertScriptSuccess("integerDiv(6, 3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerDiv(7, 3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerDiv(8, 3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerDiv(9, 3);", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("integerDiv(6:9, 3);", new EidosValue_Int_vector{2, 2, 2, 3});
	EidosAssertScriptSuccess("integerDiv(6, 2:6);", new EidosValue_Int_vector{3, 2, 1, 1, 1});
	EidosAssertScriptSuccess("integerDiv(8:12, 2:6);", new EidosValue_Int_vector{4, 3, 2, 2, 2});
	EidosAssertScriptSuccess("integerDiv(-6, 3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerDiv(-7, 3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerDiv(-8, 3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerDiv(-9, 3);", new EidosValue_Int_singleton(-3));
	EidosAssertScriptSuccess("integerDiv(6, -3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerDiv(7, -3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerDiv(8, -3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerDiv(9, -3);", new EidosValue_Int_singleton(-3));
	EidosAssertScriptSuccess("integerDiv(-6, -3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerDiv(-7, -3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerDiv(-8, -3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerDiv(-9, -3);", new EidosValue_Int_singleton(3));
	
	// integerMod()
	EidosAssertScriptSuccess("integerMod(6, 3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integerMod(7, 3);", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("integerMod(8, 3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerMod(9, 3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integerMod(6:9, 3);", new EidosValue_Int_vector{0, 1, 2, 0});
	EidosAssertScriptSuccess("integerMod(6, 2:6);", new EidosValue_Int_vector{0, 0, 2, 1, 0});
	EidosAssertScriptSuccess("integerMod(8:12, 2:6);", new EidosValue_Int_vector{0, 0, 2, 1, 0});
	EidosAssertScriptSuccess("integerMod(-6, 3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integerMod(-7, 3);", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("integerMod(-8, 3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerMod(-9, 3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integerMod(6, -3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integerMod(7, -3);", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("integerMod(8, -3);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("integerMod(9, -3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integerMod(-6, -3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integerMod(-7, -3);", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("integerMod(-8, -3);", new EidosValue_Int_singleton(-2));
	EidosAssertScriptSuccess("integerMod(-9, -3);", new EidosValue_Int_singleton(0));
	
	// isFinite()
	EidosAssertScriptSuccess("isFinite(0.0);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isFinite(0.05);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isFinite(INF);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFinite(NAN);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFinite(c(5/0, 0/0, 17.0));", new EidosValue_Logical{false, false, true});	// INF, NAN, normal
	EidosAssertScriptRaise("isFinite(1);", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(T);", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("isFinite(float(0));", new EidosValue_Logical());
	EidosAssertScriptRaise("isFinite(string(0));", 0, "cannot be type");
	
	// isInfinite()
	EidosAssertScriptSuccess("isInfinite(0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInfinite(0.05);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInfinite(INF);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isInfinite(NAN);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInfinite(c(5/0, 0/0, 17.0));", new EidosValue_Logical{true, false, false});	// INF, NAN, normal
	EidosAssertScriptRaise("isInfinite(1);", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(T);", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("isInfinite(float(0));", new EidosValue_Logical());
	EidosAssertScriptRaise("isInfinite(string(0));", 0, "cannot be type");
	
	// isNAN()
	EidosAssertScriptSuccess("isNAN(0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNAN(0.05);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNAN(INF);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNAN(NAN);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isNAN(c(5/0, 0/0, 17.0));", new EidosValue_Logical{false, true, false});	// INF, NAN, normal
	EidosAssertScriptRaise("isNAN(1);", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(T);", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("isNAN(float(0));", new EidosValue_Logical());
	EidosAssertScriptRaise("isNAN(string(0));", 0, "cannot be type");
	
	// log()
	EidosAssertScriptSuccess("abs(log(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log(E) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log(E^3.5) - 3.5) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(log(c(1, E, E^3.5)) - c(0, 1, 3.5))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("log(T);", 0, "cannot be type");
	EidosAssertScriptRaise("log('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("log(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("log(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("log(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("log(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("log(string(0));", 0, "cannot be type");
	
	// log10()
	EidosAssertScriptSuccess("abs(log10(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log10(10) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log10(0.001) - -3) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(log10(c(1, 10, 0.001)) - c(0, 1, -3))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("log10(T);", 0, "cannot be type");
	EidosAssertScriptRaise("log10('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("log10(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("log10(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("log10(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log10(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("log10(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("log10(string(0));", 0, "cannot be type");
	
	// log2()
	EidosAssertScriptSuccess("abs(log2(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log2(2) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log2(0.125) - -3) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(log2(c(1, 2, 0.125)) - c(0, 1, -3))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("log2(T);", 0, "cannot be type");
	EidosAssertScriptRaise("log2('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("log2(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("log2(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("log2(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log2(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("log2(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("log2(string(0));", 0, "cannot be type");
	
	// product()
	EidosAssertScriptSuccess("product(5);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("product(-5);", new EidosValue_Int_singleton(-5));
	EidosAssertScriptSuccess("product(c(-2, 7, -18, 12));", new EidosValue_Int_singleton(3024));
	EidosAssertScriptSuccess("product(c(200000000, 3000000000000, 1000));", new EidosValue_Float_singleton(6e23));
	EidosAssertScriptSuccess("product(5.5);", new EidosValue_Float_singleton(5.5));
	EidosAssertScriptSuccess("product(-5.5);", new EidosValue_Float_singleton(-5.5));
	EidosAssertScriptSuccess("product(c(-2.5, 7.5, -18.5, 12.5));", new EidosValue_Float_singleton(-2.5*7.5*-18.5*12.5));
	EidosAssertScriptRaise("product(T);", 0, "cannot be type");
	EidosAssertScriptRaise("product('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("product(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("product(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("product(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("product(integer(0));", new EidosValue_Int_singleton(1));	// product of no elements is 1 (as in R)
	EidosAssertScriptSuccess("product(float(0));", new EidosValue_Float_singleton(1.0));
	EidosAssertScriptRaise("product(string(0));", 0, "cannot be type");
	
	// round()
	EidosAssertScriptSuccess("round(5.1);", new EidosValue_Float_singleton(5.0));
	EidosAssertScriptSuccess("round(-5.1);", new EidosValue_Float_singleton(-5.0));
	EidosAssertScriptSuccess("round(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector{-2.0, 7, -19, 13});
	EidosAssertScriptRaise("round(T);", 0, "cannot be type");
	EidosAssertScriptRaise("round(5);", 0, "cannot be type");
	EidosAssertScriptRaise("round('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("round(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("round(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("round(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("round(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("round(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("round(string(0));", 0, "cannot be type");
	
	// sin()
	EidosAssertScriptSuccess("abs(sin(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(sin(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(sin(PI/2) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(sin(c(0, PI/2, PI)) - c(0, 1, 0))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("sin(T);", 0, "cannot be type");
	EidosAssertScriptRaise("sin('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sin(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sin(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sin(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sin(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("sin(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("sin(string(0));", 0, "cannot be type");
	
	// sqrt()
	EidosAssertScriptSuccess("sqrt(64);", new EidosValue_Float_singleton(8));
	EidosAssertScriptSuccess("isNAN(sqrt(-64));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sqrt(c(4, -16, 9, 1024));", new EidosValue_Float_vector{2, NAN, 3, 32});
	EidosAssertScriptSuccess("sqrt(64.0);", new EidosValue_Float_singleton(8));
	EidosAssertScriptSuccess("isNAN(sqrt(-64.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sqrt(c(4.0, -16.0, 9.0, 1024.0));", new EidosValue_Float_vector{2, NAN, 3, 32});
	EidosAssertScriptRaise("sqrt(T);", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sqrt(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("sqrt(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("sqrt(string(0));", 0, "cannot be type");
	
	// sum()
	EidosAssertScriptSuccess("sum(5);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("sum(-5);", new EidosValue_Int_singleton(-5));
	EidosAssertScriptSuccess("sum(c(-2, 7, -18, 12));", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("sum(c(200000000, 3000000000000));", new EidosValue_Int_singleton(3000200000000));
	EidosAssertScriptSuccess("sum(rep(3000000000000000000, 100));", new EidosValue_Float_singleton(3e20));
	EidosAssertScriptSuccess("sum(5.5);", new EidosValue_Float_singleton(5.5));
	EidosAssertScriptSuccess("sum(-5.5);", new EidosValue_Float_singleton(-5.5));
	EidosAssertScriptSuccess("sum(c(-2.5, 7.5, -18.5, 12.5));", new EidosValue_Float_singleton(-1));
	EidosAssertScriptSuccess("sum(T);", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("sum(c(T,F,T,F,T,T,T,F));", new EidosValue_Int_singleton(5));
	EidosAssertScriptRaise("sum('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sum(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sum(NULL);", 0, "cannot be type");
	EidosAssertScriptSuccess("sum(logical(0));", new EidosValue_Int_singleton(0));	// sum of no elements is 0 (as in R)
	EidosAssertScriptSuccess("sum(integer(0));", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("sum(float(0));", new EidosValue_Float_singleton(0.0));
	EidosAssertScriptRaise("sum(string(0));", 0, "cannot be type");
	
	// tan()
	EidosAssertScriptSuccess("abs(tan(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(tan(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(tan(PI/4) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(tan(c(0, PI/4, -PI/4)) - c(0, 1, -1))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("tan(T);", 0, "cannot be type");
	EidosAssertScriptRaise("tan('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("tan(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("tan(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("tan(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("tan(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("tan(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("tan(string(0));", 0, "cannot be type");
	
	// trunc()
	EidosAssertScriptSuccess("trunc(5.1);", new EidosValue_Float_singleton(5.0));
	EidosAssertScriptSuccess("trunc(-5.1);", new EidosValue_Float_singleton(-5.0));
	EidosAssertScriptSuccess("trunc(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector{-2.0, 7, -18, 12});
	EidosAssertScriptRaise("trunc(T);", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(5);", 0, "cannot be type");
	EidosAssertScriptRaise("trunc('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("trunc(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("trunc(string(0));", 0, "cannot be type");
	
	#pragma mark summary statistics
	
	// max()
	EidosAssertScriptSuccess("max(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("max(3);", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("max(3.5);", new EidosValue_Float_singleton(3.5));
	EidosAssertScriptSuccess("max('foo');", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("max(c(F, F, T, F, T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("max(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton(19));
	EidosAssertScriptSuccess("max(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Float_singleton(19.1));
	EidosAssertScriptSuccess("max(c('foo', 'bar', 'baz'));", new EidosValue_String_singleton("foo"));
	EidosAssertScriptRaise("max(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("max(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(string(0));", gStaticEidosValueNULL);
	
	// mean()
	EidosAssertScriptRaise("mean(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(3);", new EidosValue_Float_singleton(3));
	EidosAssertScriptSuccess("mean(3.5);", new EidosValue_Float_singleton(3.5));
	EidosAssertScriptRaise("mean('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("mean(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(c(3, 7, 19, -5, 16));", new EidosValue_Float_singleton(8));
	EidosAssertScriptSuccess("mean(c(3.3, 7.2, 19.1, -5.6, 16.0));", new EidosValue_Float_singleton(8.0));
	EidosAssertScriptRaise("mean(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("mean(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("mean(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("mean(string(0));", 0, "cannot be type");
	
	// min()
	EidosAssertScriptSuccess("min(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("min(3);", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("min(3.5);", new EidosValue_Float_singleton(3.5));
	EidosAssertScriptSuccess("min('foo');", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("min(c(F, F, T, F, T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("min(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton(-5));
	EidosAssertScriptSuccess("min(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Float_singleton(-5.8));
	EidosAssertScriptSuccess("min(c('foo', 'bar', 'baz'));", new EidosValue_String_singleton("bar"));
	EidosAssertScriptRaise("min(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("min(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(string(0));", gStaticEidosValueNULL);
	
	// range()
	EidosAssertScriptRaise("range(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("range(3);", new EidosValue_Int_vector{3, 3});
	EidosAssertScriptSuccess("range(3.5);", new EidosValue_Float_vector{3.5, 3.5});
	EidosAssertScriptRaise("range('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("range(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("range(c(3, 7, 19, -5, 9));", new EidosValue_Int_vector{-5, 19});
	EidosAssertScriptSuccess("range(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Float_vector{-5.8, 19.1});
	EidosAssertScriptRaise("range(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("range(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("range(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("range(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("range(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("range(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("range(string(0));", 0, "cannot be type");
	
	// sd()
	EidosAssertScriptRaise("sd(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(3);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(3.5);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sd(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(c(2, 3, 2, 8, 0));", new EidosValue_Float_singleton(3));
	EidosAssertScriptSuccess("sd(c(9.1, 5.1, 5.1, 4.1, 7.1));", new EidosValue_Float_singleton(2.0));
	EidosAssertScriptRaise("sd(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sd(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd(string(0));", 0, "cannot be type");
	
	#pragma mark vector construction
	
	// c()
	EidosAssertScriptSuccess("c();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("c(NULL);", 0, "NULL is not allowed");
	EidosAssertScriptSuccess("c(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("c(3);", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("c(3.1);", new EidosValue_Float_singleton(3.1));
	EidosAssertScriptSuccess("c('foo');", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("c(_Test(7))._yolk;", new EidosValue_Int_singleton(7));
	EidosAssertScriptRaise("c(NULL, NULL);", 0, "NULL is not allowed");
	EidosAssertScriptSuccess("c(T, F, T, T, T, F);", new EidosValue_Logical{true, false, true, true, true, false});
	EidosAssertScriptSuccess("c(3, 7, 19, -5, 9);", new EidosValue_Int_vector{3, 7, 19, -5, 9});
	EidosAssertScriptSuccess("c(3.3, 7.7, 19.1, -5.8, 9.0);", new EidosValue_Float_vector{3.3, 7.7, 19.1, -5.8, 9.0});
	EidosAssertScriptSuccess("c('foo', 'bar', 'baz');", new EidosValue_String_vector{"foo", "bar", "baz"});
	EidosAssertScriptSuccess("c(_Test(7), _Test(3), _Test(-9))._yolk;", new EidosValue_Int_vector{7, 3, -9});
	EidosAssertScriptSuccess("c(T, c(T, F, F), T, F);", new EidosValue_Logical{true, true, false, false, true, false});
	EidosAssertScriptSuccess("c(3, 7, c(17, -2), -5, 9);", new EidosValue_Int_vector{3, 7, 17, -2, -5, 9});
	EidosAssertScriptSuccess("c(3.3, 7.7, c(17.1, -2.9), -5.8, 9.0);", new EidosValue_Float_vector{3.3, 7.7, 17.1, -2.9, -5.8, 9.0});
	EidosAssertScriptSuccess("c('foo', c('bar', 'bar2', 'bar3'), 'baz');", new EidosValue_String_vector{"foo", "bar", "bar2", "bar3", "baz"});
	EidosAssertScriptSuccess("c(T, 3, F, 7);", new EidosValue_Int_vector{1, 3, 0, 7});
	EidosAssertScriptSuccess("c(T, 3, F, 7.1);", new EidosValue_Float_vector{1, 3, 0, 7.1});
	EidosAssertScriptSuccess("c(T, 3, 'bar', 7.1);", new EidosValue_String_vector{"T", "3", "bar", "7.1"});
	EidosAssertScriptRaise("c(T, NULL);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(3, NULL);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(3.1, NULL);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c('foo', NULL);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(_Test(7), NULL);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(NULL, T);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(NULL, 3);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(NULL, 3.1);", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(NULL, 'foo');", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(NULL, _Test(7));", 0, "NULL is not allowed");
	EidosAssertScriptRaise("c(T, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c(3, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c(3.1, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c('foo', _Test(7));", 0, "cannot be mixed");
	
	// float()
	EidosAssertScriptSuccess("float(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("float(1);", new EidosValue_Float_singleton(0.0));
	EidosAssertScriptSuccess("float(2);", new EidosValue_Float_vector{0.0, 0.0});
	EidosAssertScriptSuccess("float(5);", new EidosValue_Float_vector{0.0, 0.0, 0.0, 0.0, 0.0});
	EidosAssertScriptRaise("float(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("float(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("float(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("float(integer(0));", 0, "must be a singleton");
	
	// integer()
	EidosAssertScriptSuccess("integer(0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("integer(1);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("integer(2);", new EidosValue_Int_vector{0, 0});
	EidosAssertScriptSuccess("integer(5);", new EidosValue_Int_vector{0, 0, 0, 0, 0});
	EidosAssertScriptRaise("integer(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("integer(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("integer(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("integer(integer(0));", 0, "must be a singleton");
	
	// logical()
	EidosAssertScriptSuccess("logical(0);", new EidosValue_Logical());
	EidosAssertScriptSuccess("logical(1);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("logical(2);", new EidosValue_Logical{false, false});
	EidosAssertScriptSuccess("logical(5);", new EidosValue_Logical{false, false, false, false, false});
	EidosAssertScriptRaise("logical(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("logical(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("logical(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("logical(integer(0));", 0, "must be a singleton");
	
	// object()
	EidosAssertScriptSuccess("object();", new EidosValue_Object_vector());
	EidosAssertScriptRaise("object(NULL);", 0, "requires at most 0");
	EidosAssertScriptRaise("object(integer(0));", 0, "requires at most 0");
	
	// rbinom()
	EidosAssertScriptSuccess("rbinom(0, 10, 0.5);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("rbinom(3, 10, 0.0);", new EidosValue_Int_vector{0, 0, 0});
	EidosAssertScriptSuccess("rbinom(3, 10, 1.0);", new EidosValue_Int_vector{10, 10, 10});
	EidosAssertScriptSuccess("rbinom(3, 0, 0.0);", new EidosValue_Int_vector{0, 0, 0});
	EidosAssertScriptSuccess("rbinom(3, 0, 1.0);", new EidosValue_Int_vector{0, 0, 0});
	EidosAssertScriptSuccess("setSeed(1); rbinom(5, 10, 0.5);", new EidosValue_Int_vector{4, 8, 5, 3, 4});
	EidosAssertScriptSuccess("setSeed(2); rbinom(5, 10, 0.5);", new EidosValue_Int_vector{7, 6, 3, 6, 3});
	EidosAssertScriptSuccess("setSeed(3); rbinom(5, 1000, 0.01);", new EidosValue_Int_vector{11, 16, 10, 14, 10});
	EidosAssertScriptSuccess("setSeed(4); rbinom(5, 1000, 0.99);", new EidosValue_Int_vector{992, 990, 995, 991, 995});
	EidosAssertScriptSuccess("setSeed(5); rbinom(3, 100, c(0.1, 0.5, 0.9));", new EidosValue_Int_vector{7, 50, 87});
	EidosAssertScriptSuccess("setSeed(6); rbinom(3, c(10, 30, 50), 0.5);", new EidosValue_Int_vector{6, 12, 26});
	EidosAssertScriptRaise("rbinom(-1, 10, 0.5);", 0, "requires n to be");
	EidosAssertScriptRaise("rbinom(3, -1, 0.5);", 0, "requires size >= 0");
	EidosAssertScriptRaise("rbinom(3, 10, -0.1);", 0, "in [0.0, 1.0]");
	EidosAssertScriptRaise("rbinom(3, 10, 1.1);", 0, "in [0.0, 1.0]");
	EidosAssertScriptRaise("rbinom(3, 10, c(0.1, 0.2));", 0, "to be of length 1 or n");
	EidosAssertScriptRaise("rbinom(3, c(10, 12), 0.5);", 0, "to be of length 1 or n");
	
	// rep()
	EidosAssertScriptRaise("rep(NULL, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(T, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(3, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(3.5, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep('foo', -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(_Test(7), -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptSuccess("rep(NULL, 0);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("rep(T, 0);", new EidosValue_Logical());
	EidosAssertScriptSuccess("rep(3, 0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("rep(3.5, 0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("rep('foo', 0);", new EidosValue_String_vector());
	EidosAssertScriptSuccess("rep(_Test(7), 0);", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("rep(NULL, 2);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("rep(T, 2);", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("rep(3, 2);", new EidosValue_Int_vector{3, 3});
	EidosAssertScriptSuccess("rep(3.5, 2);", new EidosValue_Float_vector{3.5, 3.5});
	EidosAssertScriptSuccess("rep('foo', 2);", new EidosValue_String_vector{"foo", "foo"});
	EidosAssertScriptSuccess("rep(_Test(7), 2)._yolk;", new EidosValue_Int_vector{7, 7});
	EidosAssertScriptSuccess("rep(c(T, F), 2);", new EidosValue_Logical{true, false, true, false});
	EidosAssertScriptSuccess("rep(c(3, 7), 2);", new EidosValue_Int_vector{3, 7, 3, 7});
	EidosAssertScriptSuccess("rep(c(3.5, 9.1), 2);", new EidosValue_Float_vector{3.5, 9.1, 3.5, 9.1});
	EidosAssertScriptSuccess("rep(c('foo', 'bar'), 2);", new EidosValue_String_vector{"foo", "bar", "foo", "bar"});
	EidosAssertScriptSuccess("rep(c(_Test(7), _Test(2)), 2)._yolk;", new EidosValue_Int_vector{7, 2, 7, 2});
	EidosAssertScriptSuccess("rep(logical(0), 5);", new EidosValue_Logical());
	EidosAssertScriptSuccess("rep(integer(0), 5);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("rep(float(0), 5);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("rep(string(0), 5);", new EidosValue_String_vector());
	EidosAssertScriptSuccess("rep(object(), 5);", new EidosValue_Object_vector());
	EidosAssertScriptRaise("rep(object(), c(5, 3));", 0, "must be a singleton");
	EidosAssertScriptRaise("rep(object(), integer(0));", 0, "must be a singleton");
	
	// repEach()
	EidosAssertScriptRaise("repEach(NULL, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(T, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(3, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(3.5, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach('foo', -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(_Test(7), -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptSuccess("repEach(NULL, 0);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("repEach(T, 0);", new EidosValue_Logical());
	EidosAssertScriptSuccess("repEach(3, 0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("repEach(3.5, 0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("repEach('foo', 0);", new EidosValue_String_vector());
	EidosAssertScriptSuccess("repEach(_Test(7), 0);", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("repEach(NULL, 2);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("repEach(T, 2);", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("repEach(3, 2);", new EidosValue_Int_vector{3, 3});
	EidosAssertScriptSuccess("repEach(3.5, 2);", new EidosValue_Float_vector{3.5, 3.5});
	EidosAssertScriptSuccess("repEach('foo', 2);", new EidosValue_String_vector{"foo", "foo"});
	EidosAssertScriptSuccess("repEach(_Test(7), 2)._yolk;", new EidosValue_Int_vector{7, 7});
	EidosAssertScriptSuccess("repEach(c(T, F), 2);", new EidosValue_Logical{true, true, false, false});
	EidosAssertScriptSuccess("repEach(c(3, 7), 2);", new EidosValue_Int_vector{3, 3, 7, 7});
	EidosAssertScriptSuccess("repEach(c(3.5, 9.1), 2);", new EidosValue_Float_vector{3.5, 3.5, 9.1, 9.1});
	EidosAssertScriptSuccess("repEach(c('foo', 'bar'), 2);", new EidosValue_String_vector{"foo", "foo", "bar", "bar"});
	EidosAssertScriptSuccess("repEach(c(_Test(7), _Test(2)), 2)._yolk;", new EidosValue_Int_vector{7, 7, 2, 2});
	EidosAssertScriptRaise("repEach(NULL, c(2,3));", 0, "requires that parameter");
	EidosAssertScriptSuccess("repEach(c(T, F), c(2,3));", new EidosValue_Logical{true, true, false, false, false});
	EidosAssertScriptSuccess("repEach(c(3, 7), c(2,3));", new EidosValue_Int_vector{3, 3, 7, 7, 7});
	EidosAssertScriptSuccess("repEach(c(3.5, 9.1), c(2,3));", new EidosValue_Float_vector{3.5, 3.5, 9.1, 9.1, 9.1});
	EidosAssertScriptSuccess("repEach(c('foo', 'bar'), c(2,3));", new EidosValue_String_vector{"foo", "foo", "bar", "bar", "bar"});
	EidosAssertScriptSuccess("repEach(c(_Test(7), _Test(2)), c(2,3))._yolk;", new EidosValue_Int_vector{7, 7, 2, 2, 2});
	EidosAssertScriptRaise("repEach(NULL, c(2,-1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(T, F), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c(3, 7), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c(3.5, 9.1), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c('foo', 'bar'), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c(_Test(7), _Test(2)), c(2,-1))._yolk;", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(NULL, c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(T, F), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(3, 7), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(3.5, 9.1), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c('foo', 'bar'), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(_Test(7), _Test(2)), c(2,3,1))._yolk;", 0, "requires that parameter");
	EidosAssertScriptSuccess("repEach(logical(0), 5);", new EidosValue_Logical());
	EidosAssertScriptSuccess("repEach(integer(0), 5);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("repEach(float(0), 5);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("repEach(string(0), 5);", new EidosValue_String_vector());
	EidosAssertScriptSuccess("repEach(object(), 5);", new EidosValue_Object_vector());
	EidosAssertScriptRaise("repEach(object(), c(5, 3));", 0, "requires that parameter");
	EidosAssertScriptSuccess("repEach(object(), integer(0));", new EidosValue_Object_vector());
	
	// rexp()
	EidosAssertScriptSuccess("rexp(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("setSeed(1); (rexp(3) - c(0.206919, 3.01675, 0.788416)) < 0.000001;", new EidosValue_Logical{true, true, true});
	EidosAssertScriptSuccess("setSeed(2); (rexp(3, 0.1) - c(20.7, 12.2, 0.9)) < 0.1;", new EidosValue_Logical{true, true, true});
	EidosAssertScriptSuccess("setSeed(3); (rexp(3, 0.00001) - c(95364.3, 307170.0, 74334.9)) < 0.1;", new EidosValue_Logical{true, true, true});
	EidosAssertScriptSuccess("setSeed(4); (rexp(3, c(0.1, 0.01, 0.001)) - c(2.8, 64.6, 58.8)) < 0.1;", new EidosValue_Logical{true, true, true});
	EidosAssertScriptRaise("rexp(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rexp(3, 0.0);", 0, "requires rate > 0.0");
	EidosAssertScriptRaise("rexp(3, 0.0);", 0, "requires rate > 0.0");
	EidosAssertScriptRaise("rexp(3, c(0.1, 0.2));", 0, "requires rate to be");
	
	// rnorm()
	EidosAssertScriptSuccess("rnorm(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("rnorm(3, 0, 0);", new EidosValue_Float_vector{0.0, 0.0, 0.0});
	EidosAssertScriptSuccess("rnorm(3, 1, 0);", new EidosValue_Float_vector{1.0, 1.0, 1.0});
	EidosAssertScriptSuccess("setSeed(1); (rnorm(2) - c(-0.785386, 0.132009)) < 0.000001;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(2); (rnorm(2, 10.0) - c(10.38, 10.26)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(3); (rnorm(2, 10.0, 100.0) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(4); (rnorm(2, c(-10, 10), 100.0) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(5); (rnorm(2, 10.0, c(0.1, 10)) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptRaise("rnorm(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rnorm(1, 0, -1);", 0, "requires sd >= 0.0");
	EidosAssertScriptRaise("rnorm(2, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("rnorm(2, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	
	// rpois()
	EidosAssertScriptSuccess("rpois(0, 1.0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("setSeed(1); rpois(5, 1.0);", new EidosValue_Int_vector{0, 2, 0, 1, 1});
	EidosAssertScriptSuccess("setSeed(2); rpois(5, 0.2);", new EidosValue_Int_vector{1, 0, 0, 0, 0});
	EidosAssertScriptSuccess("setSeed(3); rpois(5, 10000);", new EidosValue_Int_vector{10205, 10177, 10094, 10227, 9875});
	EidosAssertScriptSuccess("setSeed(4); rpois(5, c(1, 10, 100, 1000, 10000));", new EidosValue_Int_vector{0, 8, 97, 994, 9911});
	EidosAssertScriptRaise("rpois(-1, 1.0);", 0, "requires n to be");
	EidosAssertScriptRaise("rpois(0, 0.0);", 0, "requires lambda");
	EidosAssertScriptRaise("setSeed(4); rpois(5, c(1, 10, 100, 1000));", 12, "requires lambda");
	
	// runif()
	EidosAssertScriptSuccess("runif(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("runif(3, 0, 0);", new EidosValue_Float_vector{0.0, 0.0, 0.0});
	EidosAssertScriptSuccess("runif(3, 1, 1);", new EidosValue_Float_vector{1.0, 1.0, 1.0});
	EidosAssertScriptSuccess("setSeed(1); (runif(2) - c(0.186915, 0.951040)) < 0.000001;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(2); (runif(2, 0.5) - c(0.93, 0.85)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(3); (runif(2, 10.0, 100.0) - c(65.31, 95.82)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(4); (runif(2, c(-100, 1), 10.0) - c(-72.52, 5.28)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("setSeed(5); (runif(2, -10.0, c(1, 1000)) - c(-8.37, 688.97)) < 0.01;", new EidosValue_Logical{true, true});
	EidosAssertScriptRaise("runif(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("runif(1, 0, -1);", 0, "requires min");
	EidosAssertScriptRaise("runif(2, c(-10, 10, 1), 100.0);", 0, "requires min");
	EidosAssertScriptRaise("runif(2, -10.0, c(0.1, 10, 1));", 0, "requires max");
	
	// sample() – since this function treats parameter x type-agnostically, we'll test integers only (and NULL a little bit)
	EidosAssertScriptSuccess("sample(NULL, 0, T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sample(NULL, 0, F);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sample(NULL, 1, T);", 0, "insufficient elements");
	EidosAssertScriptRaise("sample(NULL, 1, F);", 0, "insufficient elements");
	EidosAssertScriptRaise("sample(1:5, -1, T);", 0, "requires a sample size");
	EidosAssertScriptRaise("sample(1:5, -1, F);", 0, "requires a sample size");
	EidosAssertScriptSuccess("sample(integer(0), 0, T);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sample(integer(0), 0, F);", new EidosValue_Int_vector());
	EidosAssertScriptRaise("sample(integer(0), 1, T);", 0, "insufficient elements");
	EidosAssertScriptRaise("sample(integer(0), 1, F);", 0, "insufficient elements");
	EidosAssertScriptSuccess("sample(5, 1, T);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("sample(5, 1, F);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("sample(5, 2, T);", new EidosValue_Int_vector{5, 5});
	EidosAssertScriptRaise("sample(5, 2, F);", 0, "insufficient elements");
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, T);", new EidosValue_Int_vector{1, 5, 3, 1, 2});
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, F);", new EidosValue_Int_vector{1, 5, 3, 2, 4});
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 6, T);", new EidosValue_Int_vector{1, 5, 3, 1, 2, 3});
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 6, F);", 12, "insufficient elements");
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, T, (1:5)^3);", new EidosValue_Int_vector{4, 5, 5, 3, 4});
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, F, (1:5)^3);", new EidosValue_Int_vector{4, 5, 3, 1, 2});
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, T, (0:4)^3);", new EidosValue_Int_vector{4, 5, 5, 3, 4});
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, F, (0:4)^3);", 12, "weights summing to");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, -1:3);", 12, "requires all weights to be");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, 1:6);", 12, "to be the same length");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, 1);", 12, "to be the same length");
	
	// seq()
	EidosAssertScriptSuccess("seq(1, 5);", new EidosValue_Int_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("seq(5, 1);", new EidosValue_Int_vector{5, 4, 3, 2, 1});
	EidosAssertScriptSuccess("seq(1.1, 5);", new EidosValue_Float_vector{1.1, 2.1, 3.1, 4.1});
	EidosAssertScriptSuccess("seq(1, 5.1);", new EidosValue_Float_vector{1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("seq(1, 10, 2);", new EidosValue_Int_vector{1, 3, 5, 7, 9});
	EidosAssertScriptRaise("seq(1, 10, -2);", 0, "has incorrect sign");
	EidosAssertScriptSuccess("seq(10, 1, -2);", new EidosValue_Int_vector{10, 8, 6, 4, 2});
	EidosAssertScriptSuccess("(seq(1, 2, 0.2) - c(1, 1.2, 1.4, 1.6, 1.8, 2.0)) < 0.000000001;", new EidosValue_Logical{true, true, true, true, true, true});
	EidosAssertScriptRaise("seq(1, 2, -0.2);", 0, "has incorrect sign");
	EidosAssertScriptSuccess("(seq(2, 1, -0.2) - c(2.0, 1.8, 1.6, 1.4, 1.2, 1)) < 0.000000001;", new EidosValue_Logical{true, true, true, true, true, true});
	EidosAssertScriptRaise("seq('foo', 2, 1);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(1, 'foo', 2);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(2, 1, 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("seq(T, 2, 1);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(1, T, 2);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(2, 1, T);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(NULL, 2, 1);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(1, NULL, 2);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(2, 1, NULL);", 0, "cannot be type");
	
	// seqAlong()
	EidosAssertScriptSuccess("seqAlong(NULL);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("seqAlong(logical(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("seqAlong(object());", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("seqAlong(5);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("seqAlong(5.1);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("seqAlong('foo');", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("seqAlong(5:9);", new EidosValue_Int_vector{0, 1, 2, 3, 4});
	EidosAssertScriptSuccess("seqAlong(5.1:9.5);", new EidosValue_Int_vector{0, 1, 2, 3, 4});
	EidosAssertScriptSuccess("seqAlong(c('foo', 'bar', 'baz'));", new EidosValue_Int_vector{0, 1, 2});
	
	// string()
	EidosAssertScriptSuccess("string(0);", new EidosValue_String_vector());
	EidosAssertScriptSuccess("string(1);", new EidosValue_String_singleton(""));
	EidosAssertScriptSuccess("string(2);", new EidosValue_String_vector{"", ""});
	EidosAssertScriptSuccess("string(5);", new EidosValue_String_vector{"", "", "", "", ""});
	EidosAssertScriptRaise("string(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("string(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("string(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("string(integer(0));", 0, "must be a singleton");
	
	#pragma mark value inspection / manipulation
	
	// all()
	EidosAssertScriptRaise("all(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("all(0);", 0, "cannot be type");
	EidosAssertScriptRaise("all(0.5);", 0, "cannot be type");
	EidosAssertScriptRaise("all('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("all(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("all(logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(c(T,T,T,T,T,T,T,T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(c(T,T,T,T,T,T,T,F,T,T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(c(F,F,F,F,F,F,F,F,F,F));", gStaticEidosValue_LogicalF);
	
	// any()
	EidosAssertScriptRaise("any(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("any(0);", 0, "cannot be type");
	EidosAssertScriptRaise("any(0.5);", 0, "cannot be type");
	EidosAssertScriptRaise("any('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("any(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("any(logical(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("any(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("any(c(T,T,T,T,T,T,T,T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(c(T,T,T,T,T,T,T,F,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(c(F,F,F,F,F,F,F,F,F,F));", gStaticEidosValue_LogicalF);
	
	// cat() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptSuccess("cat(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(_Test(7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(NULL, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(T, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5.5, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat('foo', '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(_Test(7), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(c(T,T,F,T), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5:9, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5.5:8.9, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(c('foo', 'bar', 'baz'), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(c(_Test(7), _Test(7), _Test(7)), '$$');", gStaticEidosValueNULL);
	
	// identical()
	EidosAssertScriptSuccess("identical(NULL, NULL);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(NULL, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F, 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, 0);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(0, 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, 0.0);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(0.0, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', '');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical('', _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), _Test(0));", gStaticEidosValue_LogicalF);	// object elements not equal
	EidosAssertScriptSuccess("identical(F, c(F,F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(F,F), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(F,F), c(F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(F,T,F,T,T), c(F,T,F,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(F,T,T,T,T), c(F,T,F,T,T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(3, c(3,3));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3,3), 3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3,3), c(3,3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3,7,3,7,7), c(3,7,3,7,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3,7,7,7,7), c(3,7,3,7,7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(3.1, c(3.1,3.1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3.1,3.1), 3.1);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3.1,3.1), c(3.1,3.1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3.1,7.1,3.1,7.1,7.1), c(3.1,7.1,3.1,7.1,7.1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3.1,7.1,7.1,7.1,7.1), c(3.1,7.1,3.1,7.1,7.1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('bar', c('bar','bar'));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c('bar','bar'), 'bar');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c('bar','bar'), c('bar','bar'));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c('bar','baz','bar','baz','baz'), c('bar','baz','bar','baz','baz'));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c('bar','baz','baz','baz','baz'), c('bar','baz','bar','baz','baz'));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(3), c(_Test(3),_Test(3)));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(_Test(3),_Test(3)), _Test(3));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(_Test(3),_Test(3)), c(_Test(3),_Test(3)));", gStaticEidosValue_LogicalF);	// object elements not equal
	EidosAssertScriptSuccess("x = c(_Test(3),_Test(3)); y = x; identical(x, y);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = _Test(3); y = _Test(7); identical(c(x, y, x, x), c(x, y, x, x));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = _Test(3); y = _Test(7); identical(c(x, y, x, x), c(x, y, y, x));", gStaticEidosValue_LogicalF);
	
	// ifelse() – since this function treats parameters trueValues and falseValues type-agnostically, we'll test integers only (and NULL a little bit)
	EidosAssertScriptRaise("ifelse(NULL, integer(0), integer(0));", 0, "cannot be type");
	EidosAssertScriptRaise("ifelse(logical(0), NULL, integer(0));", 0, "to be the same type");
	EidosAssertScriptRaise("ifelse(logical(0), integer(0), NULL);", 0, "to be the same type");
	EidosAssertScriptSuccess("ifelse(logical(0), integer(0), integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptRaise("ifelse(logical(0), 5, 2);", 0, "of equal length");
	EidosAssertScriptRaise("ifelse(T, integer(0), integer(0));", 0, "of equal length");
	EidosAssertScriptRaise("ifelse(T, 5, 2:3);", 0, "of equal length");
	EidosAssertScriptRaise("ifelse(T, 5:6, 2);", 0, "of equal length");
	EidosAssertScriptRaise("ifelse(c(T,T), 5, 2);", 0, "of equal length");
	EidosAssertScriptSuccess("ifelse(T, 5, 2);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("ifelse(F, 5, 2);", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), 1:6, -6:-1);", new EidosValue_Int_vector{1, -5, -4, 4, -2, 6});
	
	// match()
	EidosAssertScriptSuccess("match(NULL, NULL);", new EidosValue_Int_vector());
	EidosAssertScriptRaise("match(NULL, F);", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, NULL);", 0, "to be the same type");
	EidosAssertScriptSuccess("match(F, F);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("match(F, T);", new EidosValue_Int_singleton(-1));
	EidosAssertScriptRaise("match(F, 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, F);", 0, "to be the same type");
	EidosAssertScriptSuccess("match(0, 0);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("match(0, 1);", new EidosValue_Int_singleton(-1));
	EidosAssertScriptRaise("match(0, 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, F);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, 0);", 0, "to be the same type");
	EidosAssertScriptSuccess("match(0.0, 0.0);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("match(0.0, 0.1);", new EidosValue_Int_singleton(-1));
	EidosAssertScriptRaise("match(0.0, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match('', NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match('', F);", 0, "to be the same type");
	EidosAssertScriptRaise("match('', 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match('', 0.0);", 0, "to be the same type");
	EidosAssertScriptSuccess("match('', '');", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("match('', 'f');", new EidosValue_Int_singleton(-1));
	EidosAssertScriptRaise("match('', _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), F);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), '');", 0, "to be the same type");
	EidosAssertScriptSuccess("match(_Test(0), _Test(0));", new EidosValue_Int_singleton(-1));							// different elements
	EidosAssertScriptSuccess("x = _Test(0); match(x, x);", new EidosValue_Int_singleton(0));
	
	EidosAssertScriptSuccess("match(c(F,T,F,F,T,T), T);", new EidosValue_Int_vector{-1, 0, -1, -1, 0, 0});
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1), 5);", new EidosValue_Int_vector{-1, -1, -1, -1, 0, -1});
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1.), 5.);", new EidosValue_Int_vector{-1, -1, -1, -1, 0, -1});
	EidosAssertScriptSuccess("match(c('bar','q','f','baz','foo','bar'), 'foo');", new EidosValue_Int_vector{-1, -1, -1, -1, 0, -1});
	EidosAssertScriptSuccess("match(c(_Test(0), _Test(1)), _Test(0));", new EidosValue_Int_vector{-1, -1});				// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x1,x2,x2,x9,x5,x1), x5);", new EidosValue_Int_vector{-1, -1, -1, -1, 0, -1});
	
	EidosAssertScriptSuccess("match(F, c(T,F));", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("match(9, c(5,1,9));", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("match(9., c(5,1,9.));", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("match('baz', c('foo','bar','baz'));", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("match(_Test(0), c(_Test(0), _Test(1)));", new EidosValue_Int_singleton(-1));	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x9), c(x5,x1,x9));", new EidosValue_Int_singleton(2));
	
	EidosAssertScriptSuccess("match(F, c(T,T));", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("match(7, c(5,1,9));", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("match(7., c(5,1,9.));", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("match('zip', c('foo','bar','baz'));", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("match(_Test(7), c(_Test(0), _Test(1)));", new EidosValue_Int_singleton(-1));	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x2), c(x5,x1,x9));", new EidosValue_Int_singleton(-1));
	
	EidosAssertScriptSuccess("match(c(F,T,F,F,T,T), c(T,T));", new EidosValue_Int_vector{-1, 0, -1, -1, 0, 0});
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1), c(5,1,9));", new EidosValue_Int_vector{1, -1, -1, 2, 0, 1});
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1.), c(5,1,9.));", new EidosValue_Int_vector{1, -1, -1, 2, 0, 1});
	EidosAssertScriptSuccess("match(c('bar','q','f','baz','foo','bar'), c('foo','bar','baz'));", new EidosValue_Int_vector{1, -1, -1, 2, 0, 1});
	EidosAssertScriptSuccess("match(c(_Test(0), _Test(1)), c(_Test(0), _Test(1)));", new EidosValue_Int_vector{-1, -1});	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x1,x2,x2,x9,x5,x1), c(x5,x1,x9));", new EidosValue_Int_vector{1, -1, -1, 2, 0, 1});
	
	// nchar()
	EidosAssertScriptRaise("nchar(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(T);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(5);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(5.5);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("nchar('');", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("nchar(' ');", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("nchar('abcde');", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("nchar('abc\tde');", new EidosValue_Int_singleton(6));
	EidosAssertScriptSuccess("nchar(c('', 'abcde', '', 'wumpus'));", new EidosValue_Int_vector{0, 5, 0, 6});
	
	// paste()
	EidosAssertScriptSuccess("paste(NULL);", new EidosValue_String_singleton(""));
	EidosAssertScriptSuccess("paste(T);", new EidosValue_String_singleton("T"));
	EidosAssertScriptSuccess("paste(5);", new EidosValue_String_singleton("5"));
	EidosAssertScriptSuccess("paste(5.5);", new EidosValue_String_singleton("5.5"));
	EidosAssertScriptSuccess("paste('foo');", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("paste(_Test(7));", new EidosValue_String_singleton("_TestElement"));
	EidosAssertScriptSuccess("paste(NULL, '$$');", new EidosValue_String_singleton(""));
	EidosAssertScriptSuccess("paste(T, '$$');", new EidosValue_String_singleton("T"));
	EidosAssertScriptSuccess("paste(5, '$$');", new EidosValue_String_singleton("5"));
	EidosAssertScriptSuccess("paste(5.5, '$$');", new EidosValue_String_singleton("5.5"));
	EidosAssertScriptSuccess("paste('foo', '$$');", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("paste(_Test(7), '$$');", new EidosValue_String_singleton("_TestElement"));
	EidosAssertScriptSuccess("paste(c(T,T,F,T), '$$');", new EidosValue_String_singleton("T$$T$$F$$T"));
	EidosAssertScriptSuccess("paste(5:9, '$$');", new EidosValue_String_singleton("5$$6$$7$$8$$9"));
	EidosAssertScriptSuccess("paste(5.5:8.9, '$$');", new EidosValue_String_singleton("5.5$$6.5$$7.5$$8.5"));
	EidosAssertScriptSuccess("paste(c('foo', 'bar', 'baz'), '$$');", new EidosValue_String_singleton("foo$$bar$$baz"));
	EidosAssertScriptSuccess("paste(c(_Test(7), _Test(7), _Test(7)), '$$');", new EidosValue_String_singleton("_TestElement$$_TestElement$$_TestElement"));
	
	// print()
	EidosAssertScriptSuccess("print(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(_Test(7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(c(T,T,F,T));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5:9);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5.5:8.9);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(c('foo', 'bar', 'baz'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(c(_Test(7), _Test(7), _Test(7)));", gStaticEidosValueNULL);
	
	// rev()
	EidosAssertScriptSuccess("rev(6:10);", new EidosValue_Int_vector{10,9,8,7,6});
	EidosAssertScriptSuccess("rev(-(6:10));", new EidosValue_Int_vector{-10,-9,-8,-7,-6});
	EidosAssertScriptSuccess("rev(c('foo','bar','baz'));", new EidosValue_String_vector{"baz","bar","foo"});
	EidosAssertScriptSuccess("rev(-1);", new EidosValue_Int_singleton(-1));
	EidosAssertScriptSuccess("rev(1.0);", new EidosValue_Float_singleton(1));
	EidosAssertScriptSuccess("rev('foo');", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("rev(6.0:10);", new EidosValue_Float_vector{10,9,8,7,6});
	EidosAssertScriptSuccess("rev(c(T,T,T,F));", new EidosValue_Logical{false, true, true, true});
	
	// size()
	EidosAssertScriptSuccess("size(NULL);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("size(logical(0));", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("size(5);", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("size(c(5.5, 8.7));", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("size(c('foo', 'bar', 'baz'));", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("size(rep(_Test(7), 4));", new EidosValue_Int_singleton(4));
	
	// sort()
	EidosAssertScriptSuccess("sort(integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sort(integer(0), T);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sort(integer(0), F);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sort(3);", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("sort(3, T);", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("sort(3, F);", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2));", new EidosValue_Int_vector{-3, 2, 5, 6, 19});
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), T);", new EidosValue_Int_vector{-3, 2, 5, 6, 19});
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), F);", new EidosValue_Int_vector{19, 6, 5, 2, -3});
	EidosAssertScriptSuccess("sort(c(T, F, T, T, F));", new EidosValue_Logical{false, false, true, true, true});
	EidosAssertScriptSuccess("sort(c(6.1, 19.3, -3.7, 5.2, 2.3));", new EidosValue_Float_vector{-3.7, 2.3, 5.2, 6.1, 19.3});
	EidosAssertScriptSuccess("sort(c('a', 'q', 'm', 'f', 'w'));", new EidosValue_String_vector{"a", "f", "m", "q", "w"});
	EidosAssertScriptRaise("sort(_Test(7));", 0, "cannot be type");
	
	// sortBy()
	EidosAssertScriptRaise("sortBy(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(T);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(5);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(9.1);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy('foo');", 0, "cannot be type");
	EidosAssertScriptSuccess("sortBy(object(), 'foo');", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk')._yolk;", new EidosValue_Int_vector{-8, 2, 3, 7, 75});
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk', T)._yolk;", new EidosValue_Int_vector{-8, 2, 3, 7, 75});
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk', F)._yolk;", new EidosValue_Int_vector{75, 7, 3, 2, -8});
	EidosAssertScriptRaise("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_foo')._yolk;", 0, "attempt to get a value");
	
	// str() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptSuccess("str(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(c(5.5, 8.7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(c('foo', 'bar', 'baz'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(rep(_Test(7), 4));", gStaticEidosValueNULL);
	
	// strsplit()
	EidosAssertScriptRaise("strsplit(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(T);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(5);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(5.6);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(string(0));", 0, "must be a singleton");
	EidosAssertScriptRaise("strsplit(string(0), '$$');", 0, "must be a singleton");
	EidosAssertScriptRaise("strsplit(c('foo', 'bar'));", 0, "must be a singleton");
	EidosAssertScriptRaise("strsplit(c('foo', 'bar'), '$$');", 0, "must be a singleton");
	EidosAssertScriptSuccess("strsplit('');", new EidosValue_String_singleton(""));
	EidosAssertScriptSuccess("strsplit('', '$$');", new EidosValue_String_singleton(""));
	EidosAssertScriptSuccess("strsplit(' ');", new EidosValue_String_vector{"", ""});
	EidosAssertScriptSuccess("strsplit('$$', '$$');", new EidosValue_String_vector{"", ""});
	EidosAssertScriptSuccess("strsplit('  ');", new EidosValue_String_vector{"", "", ""});
	EidosAssertScriptSuccess("strsplit('$$$$', '$$');", new EidosValue_String_vector{"", "", ""});
	EidosAssertScriptSuccess("strsplit('This is a test.');", new EidosValue_String_vector{"This", "is", "a", "test."});
	EidosAssertScriptSuccess("strsplit('This is a test.', '$$');", new EidosValue_String_singleton("This is a test."));
	EidosAssertScriptSuccess("strsplit('This is a test.', 'i');", new EidosValue_String_vector{"Th", "s ", "s a test."});
	EidosAssertScriptSuccess("strsplit('This is a test.', 's');", new EidosValue_String_vector{"Thi", " i", " a te", "t."});
	
	// substr()
	EidosAssertScriptSuccess("substr(string(0), 1);", new EidosValue_String_vector());
	EidosAssertScriptSuccess("substr(string(0), 1, 2);", new EidosValue_String_vector());
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1);", new EidosValue_String_vector{"oo", "ar", "oobaz"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 10000);", new EidosValue_String_vector{"oo", "ar", "oobaz"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 1);", new EidosValue_String_vector{"o", "a", "o"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 2);", new EidosValue_String_vector{"oo", "ar", "oo"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 3);", new EidosValue_String_vector{"oo", "ar", "oob"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3));", new EidosValue_String_vector{"oo", "r", "baz"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, c(1, 2, 3));", new EidosValue_String_vector{"o", "ar", "oob"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3), c(1, 2, 3));", new EidosValue_String_vector{"o", "r", "b"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3), c(2, 4, 6));", new EidosValue_String_vector{"oo", "r", "baz"});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 0);", new EidosValue_String_vector{"", "", ""});
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, -100, 1);", new EidosValue_String_vector{"fo", "ba", "fo"});
	EidosAssertScriptRaise("x=c('foo','bar','foobaz'); substr(x, 1, c(2, 4));", 27, "requires the size of");
	EidosAssertScriptRaise("x=c('foo','bar','foobaz'); substr(x, c(1, 2), 4);", 27, "requires the size of");
	
	// unique()
	EidosAssertScriptSuccess("unique(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("unique(logical(0));", new EidosValue_Logical());
	EidosAssertScriptSuccess("unique(integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("unique(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("unique(string(0));", new EidosValue_String_vector());
	EidosAssertScriptSuccess("unique(object());", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("unique(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("unique(5);", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("unique(3.5);", new EidosValue_Float_singleton(3.5));
	EidosAssertScriptSuccess("unique('foo');", new EidosValue_String_singleton("foo"));
	EidosAssertScriptSuccess("unique(_Test(7))._yolk;", new EidosValue_Int_singleton(7));
	EidosAssertScriptSuccess("unique(c(T,T,T,T,F,T,T));", new EidosValue_Logical{true, false});
	EidosAssertScriptSuccess("unique(c(3,5,3,9,2,3,3,7,5));", new EidosValue_Int_vector{3, 5, 9, 2, 7});
	EidosAssertScriptSuccess("unique(c(3.5,1.2,9.3,-1.0,1.2,-1.0,1.2,7.6,3.5));", new EidosValue_Float_vector{3.5, 1.2, 9.3, -1, 7.6});
	EidosAssertScriptSuccess("unique(c('foo', 'bar', 'foo', 'baz', 'baz', 'bar', 'foo'));", new EidosValue_String_vector{"foo", "bar", "baz"});
	EidosAssertScriptSuccess("unique(c(_Test(7), _Test(7), _Test(2), _Test(7), _Test(2)))._yolk;", new EidosValue_Int_vector{7, 7, 2, 7, 2});
	
	// which()
	EidosAssertScriptRaise("which(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("which(5);", 0, "cannot be type");
	EidosAssertScriptRaise("which(5.7);", 0, "cannot be type");
	EidosAssertScriptRaise("which('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("which(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("which(logical(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("which(F);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("which(T);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("which(c(T,F,F,T,F,T,F,F,T));", new EidosValue_Int_vector{0, 3, 5, 8});
	
	// whichMax()
	EidosAssertScriptSuccess("whichMax(T);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMax(3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMax(3.5);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMax('foo');", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMax(c(F, F, T, F, T));", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("whichMax(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("whichMax(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Int_singleton(2));
	EidosAssertScriptSuccess("whichMax(c('foo', 'bar', 'baz'));", new EidosValue_Int_singleton(0));
	EidosAssertScriptRaise("whichMax(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("whichMax(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(string(0));", gStaticEidosValueNULL);
	
	// whichMin()
	EidosAssertScriptSuccess("whichMin(T);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMin(3);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMin(3.5);", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMin('foo');", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMin(c(F, F, T, F, T));", new EidosValue_Int_singleton(0));
	EidosAssertScriptSuccess("whichMin(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("whichMin(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Int_singleton(3));
	EidosAssertScriptSuccess("whichMin(c('foo', 'bar', 'baz'));", new EidosValue_Int_singleton(1));
	EidosAssertScriptRaise("whichMin(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("whichMin(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(string(0));", gStaticEidosValueNULL);
	
	#pragma mark value type testing / coercion
	
	// asFloat()
	EidosAssertScriptSuccess("asFloat(-1:3);", new EidosValue_Float_vector{-1,0,1,2,3});
	EidosAssertScriptSuccess("asFloat(-1.0:3);", new EidosValue_Float_vector{-1,0,1,2,3});
	EidosAssertScriptSuccess("asFloat(c(T,F,T,F));", new EidosValue_Float_vector{1,0,1,0});
	EidosAssertScriptSuccess("asFloat(c('1','2','3'));", new EidosValue_Float_vector{1,2,3});
	EidosAssertScriptRaise("asFloat('foo');", 0, "could not be represented");
	
	// asInteger()
	EidosAssertScriptSuccess("asInteger(-1:3);", new EidosValue_Int_vector{-1,0,1,2,3});
	EidosAssertScriptSuccess("asInteger(-1.0:3);", new EidosValue_Int_vector{-1,0,1,2,3});
	EidosAssertScriptSuccess("asInteger(c(T,F,T,F));", new EidosValue_Int_vector{1,0,1,0});
	EidosAssertScriptSuccess("asInteger(c('1','2','3'));", new EidosValue_Int_vector{1,2,3});
	EidosAssertScriptRaise("asInteger('foo');", 0, "could not be represented");
	
	// asInteger() overflow tests; these may be somewhat platform-dependent but I doubt it will bite us
	EidosAssertScriptRaise("asInteger(asFloat(9223372036854775807));", 0, "too large to be converted");																// the double representation is larger than INT64_MAX
	EidosAssertScriptRaise("asInteger(asFloat(9223372036854775807-511));", 0, "too large to be converted");															// the same double representation as previous
	EidosAssertScriptSuccess("asInteger(asFloat(9223372036854775807-512));", new EidosValue_Int_singleton(9223372036854774784));	// 9223372036854774784 == 9223372036854775807-1023, the closest value to INT64_MAX that double can represent
	EidosAssertScriptSuccess("asInteger(asFloat(-9223372036854775807 - 1));", new EidosValue_Int_singleton(INT64_MIN));			// the double representation is exact
	EidosAssertScriptSuccess("asInteger(asFloat(-9223372036854775807 - 1) - 1024);", new EidosValue_Int_singleton(INT64_MIN));	// the same double representation as previous; the closest value to INT64_MIN that double can represent
	EidosAssertScriptRaise("asInteger(asFloat(-9223372036854775807 - 1) - 1025);", 0, "too large to be converted");													// overflow on cast
	EidosAssertScriptRaise("asInteger(asFloat(c(9223372036854775807, 0)));", 0, "too large to be converted");																// the double representation is larger than INT64_MAX
	EidosAssertScriptRaise("asInteger(asFloat(c(9223372036854775807, 0)-511));", 0, "too large to be converted");															// the same double representation as previous
	EidosAssertScriptSuccess("asInteger(asFloat(c(9223372036854775807, 0)-512));", new EidosValue_Int_vector{9223372036854774784, -512});	// 9223372036854774784 == 9223372036854775807-1023, the closest value to INT64_MAX that double can represent
	EidosAssertScriptSuccess("asInteger(asFloat(c(-9223372036854775807, 0) - 1));", new EidosValue_Int_vector{INT64_MIN, -1});			// the double representation is exact
	EidosAssertScriptSuccess("asInteger(asFloat(c(-9223372036854775807, 0) - 1) - 1024);", new EidosValue_Int_vector{INT64_MIN, -1025});	// the same double representation as previous; the closest value to INT64_MIN that double can represent
	EidosAssertScriptRaise("asInteger(asFloat(c(-9223372036854775807, 0) - 1) - 1025);", 0, "too large to be converted");													// overflow on cast
	
	// asLogical()
	EidosAssertScriptSuccess("asLogical(-1:3);", new EidosValue_Logical{true,false,true,true,true});
	EidosAssertScriptSuccess("asLogical(-1.0:3);", new EidosValue_Logical{true,false,true,true,true});
	EidosAssertScriptSuccess("asLogical(c(T,F,T,F));", new EidosValue_Logical{true,false,true,false});
	EidosAssertScriptSuccess("asLogical(c('foo','bar',''));", new EidosValue_Logical{true,true,false});
	
	// asString()
	EidosAssertScriptSuccess("asString(-1:3);", new EidosValue_String_vector{"-1","0","1","2","3"});
	EidosAssertScriptSuccess("asString(-1.0:3);", new EidosValue_String_vector{"-1","0","1","2","3"});
	EidosAssertScriptSuccess("asString(c(T,F,T,F));", new EidosValue_String_vector{"T","F","T","F"});
	EidosAssertScriptSuccess("asString(c('1','2','3'));", new EidosValue_String_vector{"1","2","3"});
	
	// elementType()
	EidosAssertScriptSuccess("elementType(NULL);", new EidosValue_String_singleton("NULL"));
	EidosAssertScriptSuccess("elementType(T);", new EidosValue_String_singleton("logical"));
	EidosAssertScriptSuccess("elementType(3);", new EidosValue_String_singleton("integer"));
	EidosAssertScriptSuccess("elementType(3.5);", new EidosValue_String_singleton("float"));
	EidosAssertScriptSuccess("elementType('foo');", new EidosValue_String_singleton("string"));
	EidosAssertScriptSuccess("elementType(_Test(7));", new EidosValue_String_singleton("_TestElement"));
	EidosAssertScriptSuccess("elementType(object());", new EidosValue_String_singleton("undefined"));
	
	// isFloat()
	EidosAssertScriptSuccess("isFloat(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(3.5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isFloat('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(object());", gStaticEidosValue_LogicalF);
	
	// isInteger()
	EidosAssertScriptSuccess("isInteger(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(3);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isInteger(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(object());", gStaticEidosValue_LogicalF);
	
	// isLogical()
	EidosAssertScriptSuccess("isLogical(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isLogical(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(object());", gStaticEidosValue_LogicalF);
	
	// isNULL()
	EidosAssertScriptSuccess("isNULL(NULL);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isNULL(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(object());", gStaticEidosValue_LogicalF);
	
	// isObject()
	EidosAssertScriptSuccess("isObject(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(_Test(7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isObject(object());", gStaticEidosValue_LogicalT);
	
	// isString()
	EidosAssertScriptSuccess("isString(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString('foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isString(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(object());", gStaticEidosValue_LogicalF);
	
	// type()
	EidosAssertScriptSuccess("type(NULL);", new EidosValue_String_singleton("NULL"));
	EidosAssertScriptSuccess("type(T);", new EidosValue_String_singleton("logical"));
	EidosAssertScriptSuccess("type(3);", new EidosValue_String_singleton("integer"));
	EidosAssertScriptSuccess("type(3.5);", new EidosValue_String_singleton("float"));
	EidosAssertScriptSuccess("type('foo');", new EidosValue_String_singleton("string"));
	EidosAssertScriptSuccess("type(_Test(7));", new EidosValue_String_singleton("object"));
	EidosAssertScriptSuccess("type(object());", new EidosValue_String_singleton("object"));
	
	#pragma mark filesystem access
	
	// filesAtPath() – hard to know how to test this!  These tests should be true on Un*x machines, anyway – but might be disallowed by file permissions.
	EidosAssertScriptSuccess("type(filesAtPath('/tmp')) == 'string';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(filesAtPath('/') == 'bin');", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("sum(filesAtPath('/', T) == '/bin');", new EidosValue_Int_singleton(1));
	EidosAssertScriptSuccess("filesAtPath('foo_is_a_bad_path');", gStaticEidosValueNULL);
	
	// writeFile()
	EidosAssertScriptSuccess("writeFile('/tmp/EidosTest.txt', c(paste(0:4), paste(5:9)));", gStaticEidosValue_LogicalT);
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("readFile('/tmp/EidosTest.txt') == c(paste(0:4), paste(5:9));", new EidosValue_Logical{true, true});
	EidosAssertScriptSuccess("all(asInteger(strsplit(paste(readFile('/tmp/EidosTest.txt')))) == 0:9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("readFile('foo_is_a_bad_path.txt');", gStaticEidosValueNULL);
	
	#pragma mark miscellaneous
	
	// apply()
	EidosAssertScriptSuccess("x=integer(0); apply(x, 'applyValue^2;');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1:5; apply(x, 'applyValue^2;');", new EidosValue_Float_vector{1, 4, 9, 16, 25});
	EidosAssertScriptSuccess("x=1:5; apply(x, 'product(1:applyValue);');", new EidosValue_Int_vector{1, 2, 6, 24, 120});
	EidosAssertScriptSuccess("x=1:3; apply(x, \"rep(''+applyValue, applyValue);\");", new EidosValue_String_vector{"1", "2", "2", "3", "3", "3"});
	EidosAssertScriptSuccess("x=1:5; apply(x, \"paste(rep(''+applyValue, applyValue), '');\");", new EidosValue_String_vector{"1", "22", "333", "4444", "55555"});
	EidosAssertScriptSuccess("x=1:10; apply(x, 'if (applyValue % 2) applyValue;');", new EidosValue_Int_vector{1, 3, 5, 7, 9});
	EidosAssertScriptSuccess("x=1:5; apply(x, 'y=applyValue;'); y;", new EidosValue_Int_singleton(5));
	EidosAssertScriptSuccess("x=1:5; apply(x, 'y=applyValue; y;');", new EidosValue_Int_vector{1, 2, 3, 4, 5});
	
	// date()
	EidosAssertScriptSuccess("size(strsplit(date(), '-'));", new EidosValue_Int_singleton(3));
	EidosAssertScriptRaise("date(NULL);", 0, "requires at most");
	EidosAssertScriptRaise("date(T);", 0, "requires at most");
	EidosAssertScriptRaise("date(3);", 0, "requires at most");
	EidosAssertScriptRaise("date(3.5);", 0, "requires at most");
	EidosAssertScriptRaise("date('foo');", 0, "requires at most");
	EidosAssertScriptRaise("date(_Test(7));", 0, "requires at most");
	
	// executeLambda()
	EidosAssertScriptSuccess("x=7; executeLambda('x^2;');", new EidosValue_Float_singleton(49));
	EidosAssertScriptRaise("x=7; executeLambda('x^2');", 5, "unexpected token");
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'));", 5, "must be a singleton");
	EidosAssertScriptRaise("x=7; executeLambda(string(0));", 5, "must be a singleton");
	EidosAssertScriptSuccess("x=7; executeLambda('x=x^2+4;'); x;", new EidosValue_Float_singleton(53));
	EidosAssertScriptRaise("executeLambda(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(_Test(7));", 0, "cannot be type");
	
	// function()
	EidosAssertScriptSuccess("function();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function('function');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function('foo');", gStaticEidosValueNULL);	// does not throw at present
	EidosAssertScriptRaise("function(string(0));", 0, "must be a singleton");
	EidosAssertScriptRaise("function(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("function(T);", 0, "cannot be type");
	EidosAssertScriptRaise("function(3);", 0, "cannot be type");
	EidosAssertScriptRaise("function(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("function(_Test(7));", 0, "cannot be type");
	
	// ls()
	EidosAssertScriptSuccess("ls();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("ls(NULL);", 0, "requires at most");
	EidosAssertScriptRaise("ls(T);", 0, "requires at most");
	EidosAssertScriptRaise("ls(3);", 0, "requires at most");
	EidosAssertScriptRaise("ls(3.5);", 0, "requires at most");
	EidosAssertScriptRaise("ls('foo');", 0, "requires at most");
	EidosAssertScriptRaise("ls(_Test(7));", 0, "requires at most");
	
	// license()
	EidosAssertScriptSuccess("license();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("license(NULL);", 0, "requires at most");
	EidosAssertScriptRaise("license(T);", 0, "requires at most");
	EidosAssertScriptRaise("license(3);", 0, "requires at most");
	EidosAssertScriptRaise("license(3.5);", 0, "requires at most");
	EidosAssertScriptRaise("license('foo');", 0, "requires at most");
	EidosAssertScriptRaise("license(_Test(7));", 0, "requires at most");
	
	// rm()
	EidosAssertScriptRaise("x=37; rm('x'); x;", 15, "undefined identifier");
	EidosAssertScriptSuccess("x=37; rm('y'); x;", new EidosValue_Int_singleton(37));
	EidosAssertScriptRaise("x=37; rm(); x;", 12, "undefined identifier");
	EidosAssertScriptRaise("rm(3);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("rm(T);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(F);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(INF);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(NAN);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(E);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(PI);", 0, "cannot be type");

	// setSeed()
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(5); y=runif(10); all(x==y);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(6); y=runif(10); all(x==y);", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("setSeed(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(T);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(_Test(7));", 0, "cannot be type");
	
	// getSeed()
	EidosAssertScriptSuccess("setSeed(13); getSeed();", new EidosValue_Int_singleton(13));
	EidosAssertScriptSuccess("setSeed(13); setSeed(7); getSeed();", new EidosValue_Int_singleton(7));
	EidosAssertScriptRaise("getSeed(NULL);", 0, "requires at most");
	EidosAssertScriptRaise("getSeed(T);", 0, "requires at most");
	EidosAssertScriptRaise("getSeed(3);", 0, "requires at most");
	EidosAssertScriptRaise("getSeed(3.5);", 0, "requires at most");
	EidosAssertScriptRaise("getSeed('foo');", 0, "requires at most");
	EidosAssertScriptRaise("getSeed(_Test(7));", 0, "requires at most");
	
	// stop()
	EidosAssertScriptRaise("stop();", 0, "stop() called");
	EidosAssertScriptRaise("stop('Error');", 0, "stop() called");
	EidosAssertScriptRaise("stop(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(T);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(_Test(7));", 0, "cannot be type");
	
	// time()
	EidosAssertScriptSuccess("size(strsplit(time(), ':'));", new EidosValue_Int_singleton(3));
	EidosAssertScriptRaise("time(NULL);", 0, "requires at most");
	EidosAssertScriptRaise("time(T);", 0, "requires at most");
	EidosAssertScriptRaise("time(3);", 0, "requires at most");
	EidosAssertScriptRaise("time(3.5);", 0, "requires at most");
	EidosAssertScriptRaise("time('foo');", 0, "requires at most");
	EidosAssertScriptRaise("time(_Test(7));", 0, "requires at most");
	
	// version()
	EidosAssertScriptSuccess("version();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("version(NULL);", 0, "requires at most");
	EidosAssertScriptRaise("version(T);", 0, "requires at most");
	EidosAssertScriptRaise("version(3);", 0, "requires at most");
	EidosAssertScriptRaise("version(3.5);", 0, "requires at most");
	EidosAssertScriptRaise("version('foo');", 0, "requires at most");
	EidosAssertScriptRaise("version(_Test(7));", 0, "requires at most");
	
#pragma mark code examples
	
	// Fibonacci sequence; see Eidos manual section 2.6.1-ish
	EidosAssertScriptSuccess(	"fib = c(1, 1);												\
								while (size(fib) < 20)										\
								{															\
									next_fib = fib[size(fib) - 1] + fib[size(fib) - 2];		\
									fib = c(fib, next_fib);									\
								}															\
								fib;",
							 new EidosValue_Int_vector{1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987,1597,2584,4181,6765});
	
	EidosAssertScriptSuccess(	"counter = 12;							\
								factorial = 1;							\
								do										\
								{										\
									factorial = factorial * counter;	\
									counter = counter - 1;				\
								}										\
								while (counter > 0);					\
								factorial;",
							 new EidosValue_Int_singleton(479001600));
	
	EidosAssertScriptSuccess(	"last = 200;				\
								p = integer(0);				\
								x = 2:last;					\
								lim = last^0.5;				\
								do {						\
									v = x[0];				\
									if (v > lim)			\
										break;				\
									p = c(p, v);			\
									x = x[x % v != 0];		\
								} while (T);				\
								c(p, x);",
							 new EidosValue_Int_vector{2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199});
	
	// ************************************************************************************
	//
    //	Print a summary of test results
	//
    std::cerr << endl;
    if (gEidosTestFailureCount)
        std::cerr << "\e[31mFAILURE\e[0m count: " << gEidosTestFailureCount << endl;
    std::cerr << "\e[32mSUCCESS\e[0m count: " << gEidosTestSuccessCount << endl;
	
	// If we ran tests, the random number seed has been set; let's set it back to a good seed value
	EidosInitializeRNGFromSeed(EidosGenerateSeedFromPIDAndTime());
}































































