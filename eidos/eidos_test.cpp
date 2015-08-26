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
#include <initializer_list>


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
void EidosAssertScriptRaise(const string &p_script_string, const int p_bad_position);

// Keeping records of test success / failure
static int gEidosTestSuccessCount = 0;
static int gEidosTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void EidosAssertScriptSuccess(const string &p_script_string, EidosValue *p_correct_result)
{
	EidosScript script(p_script_string);
	EidosValue *result = nullptr;
	EidosSymbolTable symbol_table;
	
	gEidosTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	try {
		script.Tokenize();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during Tokenize(): " << EidosGetTrimmedRaiseMessage() << endl;
		return;
	}
	
	try {
		script.ParseInterpreterBlockToAST();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during ParseToAST(): " << EidosGetTrimmedRaiseMessage() << endl;
		return;
	}
	
	try {
		EidosInterpreter interpreter(script, symbol_table);
		
		// note InjectIntoInterpreter() is not called here; we want a pristine environment to test the language itself
		
		result = interpreter.EvaluateInterpreterBlock(true);
		
		// We have to copy the result; it lives in the interpreter's symbol table, which will be deleted when we leave this local scope
		result = result->CopyValues();
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during EvaluateInterpreterBlock(): " << EidosGetTrimmedRaiseMessage() << endl;
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

// Instantiates and runs the script, and prints an error if the script does not cause an exception to be raised
void EidosAssertScriptRaise(const string &p_script_string, const int p_bad_position)
{
	EidosScript script(p_script_string);
	EidosSymbolTable symbol_table;
	
	try {
		script.Tokenize();
		script.ParseInterpreterBlockToAST();
		
		EidosInterpreter interpreter(script, symbol_table);
		
		// note InjectIntoInterpreter() is not called here; we want a pristine environment to test the language itself
		
		interpreter.EvaluateInterpreterBlock(true);
		
		gEidosTestFailureCount++;
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : no raise during EvaluateInterpreterBlock()." << endl;
	}
	catch (std::runtime_error err)
	{
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
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
}

void RunEidosTests(void)
{
	// Reset error counts
	gEidosTestSuccessCount = 0;
	gEidosTestFailureCount = 0;
	
	// test literals, built-in identifiers, and tokenization
	#pragma mark literals & identifiers
	EidosAssertScriptSuccess("3;", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("3e2;", new EidosValue_Int_singleton_const(300));
	EidosAssertScriptSuccess("3.1;", new EidosValue_Float_singleton_const(3.1));
	EidosAssertScriptSuccess("3.1e2;", new EidosValue_Float_singleton_const(3.1e2));
	EidosAssertScriptSuccess("3.1e-2;", new EidosValue_Float_singleton_const(3.1e-2));
	EidosAssertScriptSuccess("3.1e+2;", new EidosValue_Float_singleton_const(3.1e+2));
	EidosAssertScriptSuccess("'foo';", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("'foo\\tbar';", new EidosValue_String("foo\tbar"));
	EidosAssertScriptSuccess("'\\'foo\\'\\t\\\"bar\"';", new EidosValue_String("'foo'\t\"bar\""));
	EidosAssertScriptSuccess("\"foo\";", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("\"foo\\tbar\";", new EidosValue_String("foo\tbar"));
	EidosAssertScriptSuccess("\"\\'foo'\\t\\\"bar\\\"\";", new EidosValue_String("'foo'\t\"bar\""));
	EidosAssertScriptSuccess("T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("INF;", new EidosValue_Float_singleton_const(std::numeric_limits<double>::infinity()));
	EidosAssertScriptSuccess("NAN;", new EidosValue_Float_singleton_const(std::numeric_limits<double>::quiet_NaN()));
	EidosAssertScriptSuccess("E - exp(1) < 0.0000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("PI - asin(1)*2 < 0.0000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("foo$foo;", 3);
	EidosAssertScriptRaise("3..5;", 3);		// second period is a dot operator!
	EidosAssertScriptRaise("3ee5;", 0);
	EidosAssertScriptRaise("3e-+5;", 0);
	EidosAssertScriptRaise("3e-;", 0);
	EidosAssertScriptRaise("3e;", 0);
	EidosAssertScriptRaise("'foo' + 'foo;", 8);
	EidosAssertScriptRaise("'foo' + 'foo\\q';", 8);
	EidosAssertScriptRaise("'foo' + 'foo\\", 8);
	EidosAssertScriptRaise("'foo' + 'foo\n';", 8);
	EidosAssertScriptRaise("1e100;", 0);							// out of range for integer
	EidosAssertScriptRaise("1000000000000000000000000000;", 0);		// out of range for integer
	EidosAssertScriptRaise("1.0e100000000000;", 0);					// out of range for double
	EidosAssertScriptRaise("T = 5;", 2);
	EidosAssertScriptRaise("F = 5;", 2);
	EidosAssertScriptRaise("NULL = 5;", 5);
	EidosAssertScriptRaise("INF = 5;", 4);
	EidosAssertScriptRaise("NAN = 5;", 4);
	EidosAssertScriptRaise("E = 5;", 2);
	EidosAssertScriptRaise("PI = 5;", 3);
	
	// test some simple parsing errors
	#pragma mark parsing
	EidosAssertScriptRaise("5 + 5", 5);					// missing ;
	EidosAssertScriptRaise("{ 5;", 4);					// missing }
	EidosAssertScriptRaise("5 };", 2);					// missing {
	EidosAssertScriptRaise("(5 + 7;", 6);				// missing )
	EidosAssertScriptRaise("5 + 7);", 5);				// missing (
	EidosAssertScriptRaise("a[5;", 3);					// missing ]
	EidosAssertScriptRaise("a 5];", 2);					// missing ]
	EidosAssertScriptRaise("a(5;", 3);					// missing )
	EidosAssertScriptRaise("a 5);", 2);					// missing (
	EidosAssertScriptRaise("a.;", 2);					// missing identifier
	EidosAssertScriptRaise("if (5 T;", 6);				// missing )
	EidosAssertScriptRaise("if 5) T;", 3);				// missing (
	EidosAssertScriptRaise("if (5) else 5;", 7);		// missing statement
	EidosAssertScriptRaise("do ; (T);", 5);				// missing while
	EidosAssertScriptRaise("do ; while T);", 11);		// missing (
	EidosAssertScriptRaise("do ; while (T;", 13);		// missing )
	EidosAssertScriptRaise("while T);", 6);				// missing (
	EidosAssertScriptRaise("while (T;", 8);				// missing )
	EidosAssertScriptRaise("for;", 3);					// missing range
	EidosAssertScriptRaise("for (x);", 6);				// missing in
	EidosAssertScriptRaise("for (x in);", 9);			// missing range
	EidosAssertScriptRaise("for (in 3:5);", 5);			// missing range variable
	EidosAssertScriptRaise("for (x in 3:5;", 13);		// missing )
	EidosAssertScriptRaise("for x in 3:5) ;", 4);		// missing (
	EidosAssertScriptRaise("next 5;", 5);				// missing ;
	EidosAssertScriptRaise("break 5;", 6);				// missing ;
	
	// test some simple runtime errors
	#pragma mark runtime
	EidosAssertScriptRaise("x = y * 3;", 4);									// undefined variable referenced
	EidosAssertScriptRaise("print(y * 3);", 6);									// undefined variable referenced as function argument
	
	EidosAssertScriptRaise("x = T; x[1];", 8);									// subscript out of range (singleton logical)
	EidosAssertScriptRaise("x = T; x[-1];", 8);									// subscript out of range (singleton logical)
	EidosAssertScriptRaise("x = T; x[1] = T;", 8);								// subscript out of range in assignment (singleton logical)
	EidosAssertScriptRaise("x = T; x[-1] = T;", 8);								// subscript out of range in assignment (singleton logical)
	EidosAssertScriptRaise("x = c(T,F); x[2];", 13);							// subscript out of range (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[-1];", 13);							// subscript out of range (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[2] = F;", 13);						// subscript out of range in assignment (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[-1] = F;", 13);						// subscript out of range in assignment (vector logical)

	EidosAssertScriptRaise("x = 8; x[1];", 8);									// subscript out of range (singleton int)
	EidosAssertScriptRaise("x = 8; x[-1];", 8);									// subscript out of range (singleton int)
	EidosAssertScriptRaise("x = 8; x[1] = 7;", 8);								// subscript out of range in assignment (singleton int)
	EidosAssertScriptRaise("x = 8; x[-1] = 7;", 8);								// subscript out of range in assignment (singleton int)
	EidosAssertScriptRaise("x = 7:9; x[3];", 10);								// subscript out of range (vector int)
	EidosAssertScriptRaise("x = 7:9; x[-1];", 10);								// subscript out of range (vector int)
	EidosAssertScriptRaise("x = 7:9; x[3] = 12;", 10);							// subscript out of range in assignment (vector int)
	EidosAssertScriptRaise("x = 7:9; x[-1] = 12;", 10);							// subscript out of range in assignment (vector int)

	EidosAssertScriptRaise("x = 8.0; x[1];", 10);								// subscript out of range (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[-1];", 10);								// subscript out of range (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[1] = 7.0;", 10);							// subscript out of range in assignment (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[-1] = 7.0;", 10);						// subscript out of range in assignment (singleton float)
	EidosAssertScriptRaise("x = 7.0:9; x[3];", 12);								// subscript out of range (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[-1];", 12);							// subscript out of range (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[3] = 12.0;", 12);						// subscript out of range in assignment (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[-1] = 12.0;", 12);						// subscript out of range in assignment (vector float)

	EidosAssertScriptRaise("x = 'foo'; x[1];", 12);								// subscript out of range (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[-1];", 12);							// subscript out of range (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[1] = _Test(6);", 12);					// subscript out of range in assignment (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[-1] = _Test(6);", 12);					// subscript out of range in assignment (singleton string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[2];", 22);					// subscript out of range (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[-1];", 22);					// subscript out of range (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[2] = _Test(6);", 22);		// subscript out of range in assignment (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[-1] = _Test(6);", 22);		// subscript out of range in assignment (vector string)

	EidosAssertScriptRaise("x = _Test(8); x[1];", 15);							// subscript out of range (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[-1];", 15);							// subscript out of range (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[1] = _Test(6);", 15);				// subscript out of range in assignment (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[-1] = _Test(6);", 15);				// subscript out of range in assignment (singleton object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[2];", 23);					// subscript out of range (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[-1];", 23);					// subscript out of range (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[2] = _Test(6);", 23);		// subscript out of range in assignment (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[-1] = _Test(6);", 23);		// subscript out of range in assignment (vector object)

	
	// ************************************************************************************
	//
	//	Operator tests
	//
	
	// test vector-to-singleton comparisons for integers
	#pragma mark vectors & singletons
	EidosAssertScriptSuccess("rep(1:3, 2) == 2;", new EidosValue_Logical(false, true, false, false, true, false));
	EidosAssertScriptSuccess("rep(1:3, 2) != 2;", new EidosValue_Logical(true, false, true, true, false, true));
	EidosAssertScriptSuccess("rep(1:3, 2) < 2;", new EidosValue_Logical(true, false, false, true, false, false));
	EidosAssertScriptSuccess("rep(1:3, 2) <= 2;", new EidosValue_Logical(true, true, false, true, true, false));
	EidosAssertScriptSuccess("rep(1:3, 2) > 2;", new EidosValue_Logical(false, false, true, false, false, true));
	EidosAssertScriptSuccess("rep(1:3, 2) >= 2;", new EidosValue_Logical(false, true, true, false, true, true));
	
	EidosAssertScriptSuccess("2 == rep(1:3, 2);", new EidosValue_Logical(false, true, false, false, true, false));
	EidosAssertScriptSuccess("2 != rep(1:3, 2);", new EidosValue_Logical(true, false, true, true, false, true));
	EidosAssertScriptSuccess("2 > rep(1:3, 2);", new EidosValue_Logical(true, false, false, true, false, false));
	EidosAssertScriptSuccess("2 >= rep(1:3, 2);", new EidosValue_Logical(true, true, false, true, true, false));
	EidosAssertScriptSuccess("2 < rep(1:3, 2);", new EidosValue_Logical(false, false, true, false, false, true));
	EidosAssertScriptSuccess("2 <= rep(1:3, 2);", new EidosValue_Logical(false, true, true, false, true, true));
	
	#pragma mark -
	#pragma mark Operators
	#pragma mark -
	
	// operator +
	#pragma mark operator +
	EidosAssertScriptRaise("NULL+T;", 4);
	EidosAssertScriptRaise("NULL+0;", 4);
	EidosAssertScriptRaise("NULL+0.5;", 4);
	EidosAssertScriptRaise("NULL+'foo';", 4);
	EidosAssertScriptRaise("NULL+_Test(7);", 4);
	EidosAssertScriptRaise("NULL+(0:2);", 4);
	EidosAssertScriptRaise("T+NULL;", 1);
	EidosAssertScriptRaise("0+NULL;", 1);
	EidosAssertScriptRaise("0.5+NULL;", 3);
	EidosAssertScriptRaise("'foo'+NULL;", 5);
	EidosAssertScriptRaise("_Test(7)+NULL;", 8);
	EidosAssertScriptRaise("(0:2)+NULL;", 5);
	EidosAssertScriptRaise("+NULL;", 0);
	EidosAssertScriptSuccess("1+1;", new EidosValue_Int_singleton_const(2));
	EidosAssertScriptSuccess("1+-1;", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("(0:2)+10;", new EidosValue_Int_vector(10, 11, 12));
	EidosAssertScriptSuccess("10+(0:2);", new EidosValue_Int_vector(10, 11, 12));
	EidosAssertScriptSuccess("(15:13)+(0:2);", new EidosValue_Int_vector(15, 15, 15));
	EidosAssertScriptRaise("(15:12)+(0:2);", 7);
	EidosAssertScriptSuccess("1+1.0;", new EidosValue_Float_singleton_const(2));
	EidosAssertScriptSuccess("1.0+1;", new EidosValue_Float_singleton_const(2));
	EidosAssertScriptSuccess("1.0+-1.0;", new EidosValue_Float_singleton_const(0));
	EidosAssertScriptSuccess("(0:2.0)+10;", new EidosValue_Float_vector(10, 11, 12));
	EidosAssertScriptSuccess("10.0+(0:2);", new EidosValue_Float_vector(10, 11, 12));
	EidosAssertScriptSuccess("(15.0:13)+(0:2.0);", new EidosValue_Float_vector(15, 15, 15));
	EidosAssertScriptRaise("(15:12.0)+(0:2);", 9);
	EidosAssertScriptSuccess("'foo'+5;", new EidosValue_String("foo5"));
	EidosAssertScriptSuccess("'foo'+5.0;", new EidosValue_String("foo5"));
	EidosAssertScriptSuccess("'foo'+5.1;", new EidosValue_String("foo5.1"));
	EidosAssertScriptSuccess("5+'foo';", new EidosValue_String("5foo"));
	EidosAssertScriptSuccess("5.0+'foo';", new EidosValue_String("5foo"));
	EidosAssertScriptSuccess("5.1+'foo';", new EidosValue_String("5.1foo"));
	EidosAssertScriptSuccess("'foo'+1:3;", new EidosValue_String("foo1", "foo2", "foo3"));
	EidosAssertScriptSuccess("1:3+'foo';", new EidosValue_String("1foo", "2foo", "3foo"));
	EidosAssertScriptSuccess("'foo'+'bar';", new EidosValue_String("foobar"));
	EidosAssertScriptSuccess("'foo'+c('bar', 'baz');", new EidosValue_String("foobar", "foobaz"));
	EidosAssertScriptSuccess("c('bar', 'baz')+'foo';", new EidosValue_String("barfoo", "bazfoo"));
	EidosAssertScriptSuccess("c('bar', 'baz')+T;", new EidosValue_String("barT", "bazT"));
	EidosAssertScriptSuccess("F+c('bar', 'baz');", new EidosValue_String("Fbar", "Fbaz"));
	EidosAssertScriptRaise("T+F;", 1);
	EidosAssertScriptRaise("T+T;", 1);
	EidosAssertScriptRaise("F+F;", 1);
	EidosAssertScriptSuccess("+5;", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("+5.0;", new EidosValue_Float_singleton_const(5));
	EidosAssertScriptRaise("+'foo';", 0);
	EidosAssertScriptRaise("+T;", 0);
	EidosAssertScriptSuccess("3+4+5;", new EidosValue_Int_singleton_const(12));
	
	// operator -
	#pragma mark operator −
	EidosAssertScriptRaise("NULL-T;", 4);
	EidosAssertScriptRaise("NULL-0;", 4);
	EidosAssertScriptRaise("NULL-0.5;", 4);
	EidosAssertScriptRaise("NULL-'foo';", 4);
	EidosAssertScriptRaise("NULL-_Test(7);", 4);
	EidosAssertScriptRaise("NULL-(0:2);", 4);
	EidosAssertScriptRaise("T-NULL;", 1);
	EidosAssertScriptRaise("0-NULL;", 1);
	EidosAssertScriptRaise("0.5-NULL;", 3);
	EidosAssertScriptRaise("'foo'-NULL;", 5);
	EidosAssertScriptRaise("_Test(7)-NULL;", 8);
	EidosAssertScriptRaise("(0:2)-NULL;", 5);
	EidosAssertScriptRaise("-NULL;", 0);
	EidosAssertScriptSuccess("1-1;", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("1--1;", new EidosValue_Int_singleton_const(2));
	EidosAssertScriptSuccess("(0:2)-10;", new EidosValue_Int_vector(-10, -9, -8));
	EidosAssertScriptSuccess("10-(0:2);", new EidosValue_Int_vector(10, 9, 8));
	EidosAssertScriptSuccess("(15:13)-(0:2);", new EidosValue_Int_vector(15, 13, 11));
	EidosAssertScriptRaise("(15:12)-(0:2);", 7);
	EidosAssertScriptSuccess("1-1.0;", new EidosValue_Float_singleton_const(0));
	EidosAssertScriptSuccess("1.0-1;", new EidosValue_Float_singleton_const(0));
	EidosAssertScriptSuccess("1.0--1.0;", new EidosValue_Float_singleton_const(2));
	EidosAssertScriptSuccess("(0:2.0)-10;", new EidosValue_Float_vector(-10, -9, -8));
	EidosAssertScriptSuccess("10.0-(0:2);", new EidosValue_Float_vector(10, 9, 8));
	EidosAssertScriptSuccess("(15.0:13)-(0:2.0);", new EidosValue_Float_vector(15, 13, 11));
	EidosAssertScriptRaise("(15:12.0)-(0:2);", 9);
	EidosAssertScriptRaise("'foo'-1;", 5);
	EidosAssertScriptRaise("T-F;", 1);
	EidosAssertScriptRaise("T-T;", 1);
	EidosAssertScriptRaise("F-F;", 1);
	EidosAssertScriptSuccess("-5;", new EidosValue_Int_singleton_const(-5));
	EidosAssertScriptSuccess("-5.0;", new EidosValue_Float_singleton_const(-5));
	EidosAssertScriptRaise("-'foo';", 0);
	EidosAssertScriptRaise("-T;", 0);
	EidosAssertScriptSuccess("3-4-5;", new EidosValue_Int_singleton_const(-6));
	
    // operator *
	#pragma mark operator *
	EidosAssertScriptRaise("NULL*T;", 4);
	EidosAssertScriptRaise("NULL*0;", 4);
	EidosAssertScriptRaise("NULL*0.5;", 4);
	EidosAssertScriptRaise("NULL*'foo';", 4);
	EidosAssertScriptRaise("NULL*_Test(7);", 4);
	EidosAssertScriptRaise("NULL*(0:2);", 4);
	EidosAssertScriptRaise("T*NULL;", 1);
	EidosAssertScriptRaise("0*NULL;", 1);
	EidosAssertScriptRaise("0.5*NULL;", 3);
	EidosAssertScriptRaise("'foo'*NULL;", 5);
	EidosAssertScriptRaise("_Test(7)*NULL;", 8);
	EidosAssertScriptRaise("(0:2)*NULL;", 5);
	EidosAssertScriptRaise("*NULL;", 0);
    EidosAssertScriptSuccess("1*1;", new EidosValue_Int_singleton_const(1));
    EidosAssertScriptSuccess("1*-1;", new EidosValue_Int_singleton_const(-1));
    EidosAssertScriptSuccess("(0:2)*10;", new EidosValue_Int_vector(0, 10, 20));
    EidosAssertScriptSuccess("10*(0:2);", new EidosValue_Int_vector(0, 10, 20));
    EidosAssertScriptSuccess("(15:13)*(0:2);", new EidosValue_Int_vector(0, 14, 26));
	EidosAssertScriptRaise("(15:12)*(0:2);", 7);
    EidosAssertScriptSuccess("1*1.0;", new EidosValue_Float_singleton_const(1));
    EidosAssertScriptSuccess("1.0*1;", new EidosValue_Float_singleton_const(1));
    EidosAssertScriptSuccess("1.0*-1.0;", new EidosValue_Float_singleton_const(-1));
    EidosAssertScriptSuccess("(0:2.0)*10;", new EidosValue_Float_vector(0, 10, 20));
    EidosAssertScriptSuccess("10.0*(0:2);", new EidosValue_Float_vector(0, 10, 20));
    EidosAssertScriptSuccess("(15.0:13)*(0:2.0);", new EidosValue_Float_vector(0, 14, 26));
	EidosAssertScriptRaise("(15:12.0)*(0:2);", 9);
	EidosAssertScriptRaise("'foo'*5;", 5);
	EidosAssertScriptRaise("T*F;", 1);
	EidosAssertScriptRaise("T*T;", 1);
	EidosAssertScriptRaise("F*F;", 1);
	EidosAssertScriptRaise("*5;", 0);
	EidosAssertScriptRaise("*5.0;", 0);
	EidosAssertScriptRaise("*'foo';", 0);
	EidosAssertScriptRaise("*T;", 0);
    EidosAssertScriptSuccess("3*4*5;", new EidosValue_Int_singleton_const(60));
    
    // operator /
	#pragma mark operator /
	EidosAssertScriptRaise("NULL/T;", 4);
	EidosAssertScriptRaise("NULL/0;", 4);
	EidosAssertScriptRaise("NULL/0.5;", 4);
	EidosAssertScriptRaise("NULL/'foo';", 4);
	EidosAssertScriptRaise("NULL/_Test(7);", 4);
	EidosAssertScriptRaise("NULL/(0:2);", 4);
	EidosAssertScriptRaise("T/NULL;", 1);
	EidosAssertScriptRaise("0/NULL;", 1);
	EidosAssertScriptRaise("0.5/NULL;", 3);
	EidosAssertScriptRaise("'foo'/NULL;", 5);
	EidosAssertScriptRaise("_Test(7)/NULL;", 8);
	EidosAssertScriptRaise("(0:2)/NULL;", 5);
	EidosAssertScriptRaise("/NULL;", 0);
    EidosAssertScriptSuccess("1/1;", new EidosValue_Float_singleton_const(1));
    EidosAssertScriptSuccess("1/-1;", new EidosValue_Float_singleton_const(-1));
    EidosAssertScriptSuccess("(0:2)/10;", new EidosValue_Float_vector(0, 0.1, 0.2));
	EidosAssertScriptRaise("(15:12)/(0:2);", 7);
    EidosAssertScriptSuccess("1/1.0;", new EidosValue_Float_singleton_const(1));
    EidosAssertScriptSuccess("1.0/1;", new EidosValue_Float_singleton_const(1));
    EidosAssertScriptSuccess("1.0/-1.0;", new EidosValue_Float_singleton_const(-1));
    EidosAssertScriptSuccess("(0:2.0)/10;", new EidosValue_Float_vector(0, 0.1, 0.2));
    EidosAssertScriptSuccess("10.0/(0:2);", new EidosValue_Float_vector(std::numeric_limits<double>::infinity(), 10, 5));
    EidosAssertScriptSuccess("(15.0:13)/(0:2.0);", new EidosValue_Float_vector(std::numeric_limits<double>::infinity(), 14, 6.5));
	EidosAssertScriptRaise("(15:12.0)/(0:2);", 9);
	EidosAssertScriptRaise("'foo'/5;", 5);
	EidosAssertScriptRaise("T/F;", 1);
	EidosAssertScriptRaise("T/T;", 1);
	EidosAssertScriptRaise("F/F;", 1);
	EidosAssertScriptRaise("/5;", 0);
	EidosAssertScriptRaise("/5.0;", 0);
	EidosAssertScriptRaise("/'foo';", 0);
	EidosAssertScriptRaise("/T;", 0);
    EidosAssertScriptSuccess("3/4/5;", new EidosValue_Float_singleton_const(0.15));
	EidosAssertScriptSuccess("6/0;", new EidosValue_Float_singleton_const(std::numeric_limits<double>::infinity()));
    
    // operator %
	#pragma mark operator %
	EidosAssertScriptRaise("NULL%T;", 4);
	EidosAssertScriptRaise("NULL%0;", 4);
	EidosAssertScriptRaise("NULL%0.5;", 4);
	EidosAssertScriptRaise("NULL%'foo';", 4);
	EidosAssertScriptRaise("NULL%_Test(7);", 4);
	EidosAssertScriptRaise("NULL%(0:2);", 4);
	EidosAssertScriptRaise("T%NULL;", 1);
	EidosAssertScriptRaise("0%NULL;", 1);
	EidosAssertScriptRaise("0.5%NULL;", 3);
	EidosAssertScriptRaise("'foo'%NULL;", 5);
	EidosAssertScriptRaise("_Test(7)%NULL;", 8);
	EidosAssertScriptRaise("(0:2)%NULL;", 5);
	EidosAssertScriptRaise("%NULL;", 0);
    EidosAssertScriptSuccess("1%1;", new EidosValue_Float_singleton_const(0));
    EidosAssertScriptSuccess("1%-1;", new EidosValue_Float_singleton_const(0));
    EidosAssertScriptSuccess("(0:2)%10;", new EidosValue_Float_vector(0, 1, 2));
	EidosAssertScriptRaise("(15:12)%(0:2);", 7);
    EidosAssertScriptSuccess("1%1.0;", new EidosValue_Float_singleton_const(0));
    EidosAssertScriptSuccess("1.0%1;", new EidosValue_Float_singleton_const(0));
    EidosAssertScriptSuccess("1.0%-1.0;", new EidosValue_Float_singleton_const(0));
    EidosAssertScriptSuccess("(0:2.0)%10;", new EidosValue_Float_vector(0, 1, 2));
    EidosAssertScriptSuccess("10.0%(0:4);", new EidosValue_Float_vector(std::numeric_limits<double>::quiet_NaN(), 0, 0, 1, 2));
    EidosAssertScriptSuccess("(15.0:13)%(0:2.0);", new EidosValue_Float_vector(std::numeric_limits<double>::quiet_NaN(), 0, 1));
	EidosAssertScriptRaise("(15:12.0)%(0:2);", 9);
	EidosAssertScriptRaise("'foo'%5;", 5);
	EidosAssertScriptRaise("T%F;", 1);
	EidosAssertScriptRaise("T%T;", 1);
	EidosAssertScriptRaise("F%F;", 1);
	EidosAssertScriptRaise("%5;", 0);
	EidosAssertScriptRaise("%5.0;", 0);
	EidosAssertScriptRaise("%'foo';", 0);
	EidosAssertScriptRaise("%T;", 0);
    EidosAssertScriptSuccess("3%4%5;", new EidosValue_Float_singleton_const(3));

	// operator []
	#pragma mark operator []
	EidosAssertScriptRaise("x = 1:5; x[NULL];", 10);
	EidosAssertScriptSuccess("x = 1:5; NULL[x];", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; NULL[NULL];", gStaticEidosValueNULL);
	EidosAssertScriptRaise("x = 1:5; x[];", 11);
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)];", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("x = 1:5; x[2];", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("x = 1:5; x[2:3];", new EidosValue_Int_vector(3, 4));
	EidosAssertScriptSuccess("x = 1:5; x[c(0, 2, 4)];", new EidosValue_Int_vector(1, 3, 5));
	EidosAssertScriptSuccess("x = 1:5; x[0:4];", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("x = 1:5; x[float(0)];", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("x = 1:5; x[2.0];", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("x = 1:5; x[2.0:3];", new EidosValue_Int_vector(3, 4));
	EidosAssertScriptSuccess("x = 1:5; x[c(0.0, 2, 4)];", new EidosValue_Int_vector(1, 3, 5));
	EidosAssertScriptSuccess("x = 1:5; x[0.0:4];", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	EidosAssertScriptRaise("x = 1:5; x[logical(0)];", 10);
	EidosAssertScriptRaise("x = 1:5; x[T];", 10);
	EidosAssertScriptRaise("x = 1:5; x[c(T, T)];", 10);
	EidosAssertScriptRaise("x = 1:5; x[c(T, F, T)];", 10);
	EidosAssertScriptSuccess("x = 1:5; x[c(T, F, T, F, T)];", new EidosValue_Int_vector(1, 3, 5));
	EidosAssertScriptSuccess("x = 1:5; x[c(T, T, T, T, T)];", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("x = 1:5; x[c(F, F, F, F, F)];", new EidosValue_Int_vector());
	
	// operator = (especially in conjunction with operator [])
	#pragma mark operator = with []
	EidosAssertScriptSuccess("x = 5; x;", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("x = 1:5; x;", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 10; x;", new EidosValue_Int_vector(10, 2, 10, 4, 10));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 10; x;", new EidosValue_Int_vector(1, 2, 10, 4, 10));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 10; x;", new EidosValue_Int_vector(10, 2, 10, 4, 10));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 10; x;", new EidosValue_Int_vector(10, 2, 10, 4, 5));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 11:13; x;", new EidosValue_Int_vector(11, 2, 12, 4, 13));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 11:12; x;", new EidosValue_Int_vector(1, 2, 11, 4, 12));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 11:13; x;", new EidosValue_Int_vector(11, 2, 12, 4, 13));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 11:12; x;", new EidosValue_Int_vector(11, 2, 12, 4, 5));
	EidosAssertScriptRaise("x = 1:5; x[1:3*2 - 2][0:1] = 11:13; x;", 27);
	EidosAssertScriptRaise("x = 1:5; x[NULL] = NULL; x;", 10);
	EidosAssertScriptRaise("x = 1:5; x[NULL] = 10; x;", 10);
	EidosAssertScriptRaise("x = 1:5; x[3] = NULL; x;", 14);
	EidosAssertScriptRaise("x = 1:5; x[integer(0)] = NULL; x;", 23);
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)] = 10; x;", new EidosValue_Int_vector(1, 2, 3, 4, 5)); // assigns 10 to no indices, perfectly legal
	EidosAssertScriptRaise("x = 1:5; x[3] = integer(0); x;", 14);
	EidosAssertScriptSuccess("x = 1.0:5; x[3] = 1; x;", new EidosValue_Float_vector(1, 2, 3, 1, 5));
	EidosAssertScriptSuccess("x = c('a', 'b', 'c'); x[1] = 1; x;", new EidosValue_String("a", "1", "c"));
	EidosAssertScriptRaise("x = 1:5; x[3] = 1.5; x;", 14);
	EidosAssertScriptRaise("x = 1:5; x[3] = 'foo'; x;", 14);
	EidosAssertScriptSuccess("x = 5; x[0] = 10; x;", new EidosValue_Int_singleton_const(10));
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10.0; x;", new EidosValue_Float_singleton_const(10));
	EidosAssertScriptRaise("x = 5; x[0] = 10.0; x;", 12);
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10; x;", new EidosValue_Float_singleton_const(10));
	EidosAssertScriptSuccess("x = T; x[0] = F; x;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("x = 'foo'; x[0] = 'bar'; x;", new EidosValue_String("bar"));
	
	// operator = (especially in conjunction with operator .)
	#pragma mark operator = with .
	EidosAssertScriptSuccess("x=_Test(9); x._yolk;", new EidosValue_Int_singleton_const(9));
	EidosAssertScriptRaise("x=_Test(NULL);", 2);
	EidosAssertScriptRaise("x=_Test(9); x._yolk = NULL;", 20);
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk;", new EidosValue_Int_vector(9, 7, 9, 7));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=2; z._yolk;", new EidosValue_Int_vector(9, 2, 9, 2));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=2; z._yolk;", new EidosValue_Int_vector(9, 2, 9, 2));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[c(1,0)]._yolk=c(2, 5); z._yolk;", new EidosValue_Int_vector(5, 2, 5, 2));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[c(1,0)]=c(3, 6); z._yolk;", new EidosValue_Int_vector(6, 3, 6, 3));
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=6.5; z._yolk;", 48);
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=6.5; z._yolk;", 48);
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2:3]._yolk=6.5; z._yolk;", 50);
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[2:3]=6.5; z._yolk;", 50);
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2]=6.5; z._yolk;", 42);
	
	// operator >
	#pragma mark operator >
	EidosAssertScriptRaise("NULL>T;", 4);
	EidosAssertScriptRaise("NULL>0;", 4);
	EidosAssertScriptRaise("NULL>0.5;", 4);
	EidosAssertScriptRaise("NULL>'foo';", 4);
	EidosAssertScriptRaise("NULL>_Test(7);", 4);
	EidosAssertScriptRaise("NULL>(0:2);", 4);
	EidosAssertScriptRaise("T>NULL;", 1);
	EidosAssertScriptRaise("0>NULL;", 1);
	EidosAssertScriptRaise("0.5>NULL;", 3);
	EidosAssertScriptRaise("'foo'>NULL;", 5);
	EidosAssertScriptRaise("_Test(7)>NULL;", 8);
	EidosAssertScriptRaise("(0:2)>NULL;", 5);
	EidosAssertScriptRaise(">NULL;", 0);
	EidosAssertScriptSuccess("T > F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T > T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F > T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F > F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T > 0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T > 1;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F > 0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F > 1;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T > -5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-5 > T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T > 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 > T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T > -5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-5.0 > T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T > 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 > T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T > 'FOO';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'FOO' > T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T > 'XYZZY';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'XYZZY' > T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 > -10;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10 > 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 > -10;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10 > 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 > -10.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10.0 > 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'foo' > 'bar';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'bar' > 'foo';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("120 > '10';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("10 > '120';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("120 > '15';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("15 > '120';", new EidosValue_Logical(true));
	EidosAssertScriptRaise("_Test(9) > 5;", 9);
	EidosAssertScriptRaise("5 > _Test(9);", 2);
	EidosAssertScriptSuccess("5 > 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10.0 > -10.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 > 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 > 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 > '5';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'5' > 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'foo' > 'foo';", new EidosValue_Logical(false));
	EidosAssertScriptRaise("_Test(9) > _Test(9);", 9);
	
	// operator <
	#pragma mark operator <
	EidosAssertScriptRaise("NULL<T;", 4);
	EidosAssertScriptRaise("NULL<0;", 4);
	EidosAssertScriptRaise("NULL<0.5;", 4);
	EidosAssertScriptRaise("NULL<'foo';", 4);
	EidosAssertScriptRaise("NULL<_Test(7);", 4);
	EidosAssertScriptRaise("NULL<(0:2);", 4);
	EidosAssertScriptRaise("T<NULL;", 1);
	EidosAssertScriptRaise("0<NULL;", 1);
	EidosAssertScriptRaise("0.5<NULL;", 3);
	EidosAssertScriptRaise("'foo'<NULL;", 5);
	EidosAssertScriptRaise("_Test(7)<NULL;", 8);
	EidosAssertScriptRaise("(0:2)<NULL;", 5);
	EidosAssertScriptRaise("<NULL;", 0);
	EidosAssertScriptSuccess("T < F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T < T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F < T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F < F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T < 0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T < 1;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F < 0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F < 1;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T < -5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-5 < T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T < 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 < T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T < -5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-5.0 < T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T < 5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0 < T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T < 'FOO';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'FOO' < T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T < 'XYZZY';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'XYZZY' < T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 < -10;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10 < 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0 < -10;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10 < 5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 < -10.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10.0 < 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'foo' < 'bar';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'bar' < 'foo';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("120 < '10';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("10 < '120';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("120 < '15';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("15 < '120';", new EidosValue_Logical(false));
	EidosAssertScriptRaise("_Test(9) < 5;", 9);
	EidosAssertScriptRaise("5 < _Test(9);", 2);
	EidosAssertScriptSuccess("5 < 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10.0 < -10.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 < 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 < 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 < '5';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'5' < 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'foo' < 'foo';", new EidosValue_Logical(false));
	EidosAssertScriptRaise("_Test(9) < _Test(9);", 9);
	
	// operator >=
	#pragma mark operator >=
	EidosAssertScriptRaise("NULL>=T;", 4);
	EidosAssertScriptRaise("NULL>=0;", 4);
	EidosAssertScriptRaise("NULL>=0.5;", 4);
	EidosAssertScriptRaise("NULL>='foo';", 4);
	EidosAssertScriptRaise("NULL>=_Test(7);", 4);
	EidosAssertScriptRaise("NULL>=(0:2);", 4);
	EidosAssertScriptRaise("T>=NULL;", 1);
	EidosAssertScriptRaise("0>=NULL;", 1);
	EidosAssertScriptRaise("0.5>=NULL;", 3);
	EidosAssertScriptRaise("'foo'>=NULL;", 5);
	EidosAssertScriptRaise("_Test(7)>=NULL;", 8);
	EidosAssertScriptRaise("(0:2)>=NULL;", 5);
	EidosAssertScriptRaise(">=NULL;", 0);
	EidosAssertScriptSuccess("T >= F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T >= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F >= T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F >= F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T >= 0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T >= 1;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F >= 0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F >= 1;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T >= -5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-5 >= T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T >= 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 >= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T >= -5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-5.0 >= T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T >= 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 >= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T >= 'FOO';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'FOO' >= T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T >= 'XYZZY';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'XYZZY' >= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 >= -10;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10 >= 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 >= -10;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10 >= 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 >= -10.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10.0 >= 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'foo' >= 'bar';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'bar' >= 'foo';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("120 >= '10';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("10 >= '120';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("120 >= '15';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("15 >= '120';", new EidosValue_Logical(true));
	EidosAssertScriptRaise("_Test(9) >= 5;", 9);
	EidosAssertScriptRaise("5 >= _Test(9);", 2);
	EidosAssertScriptSuccess("5 >= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10.0 >= -10.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 >= 5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0 >= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 >= '5';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'5' >= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'foo' >= 'foo';", new EidosValue_Logical(true));
	EidosAssertScriptRaise("_Test(9) >= _Test(9);", 9);
	
	// operator <=
	#pragma mark operator <=
	EidosAssertScriptRaise("NULL<=T;", 4);
	EidosAssertScriptRaise("NULL<=0;", 4);
	EidosAssertScriptRaise("NULL<=0.5;", 4);
	EidosAssertScriptRaise("NULL<='foo';", 4);
	EidosAssertScriptRaise("NULL<=_Test(7);", 4);
	EidosAssertScriptRaise("NULL<=(0:2);", 4);
	EidosAssertScriptRaise("T<=NULL;", 1);
	EidosAssertScriptRaise("0<=NULL;", 1);
	EidosAssertScriptRaise("0.5<=NULL;", 3);
	EidosAssertScriptRaise("'foo'<=NULL;", 5);
	EidosAssertScriptRaise("_Test(7)<=NULL;", 8);
	EidosAssertScriptRaise("(0:2)<=NULL;", 5);
	EidosAssertScriptRaise("<=NULL;", 0);
	EidosAssertScriptSuccess("T <= F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T <= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F <= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F <= F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T <= 0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T <= 1;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F <= 0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F <= 1;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T <= -5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-5 <= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T <= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 <= T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T <= -5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-5.0 <= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T <= 5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0 <= T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T <= 'FOO';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'FOO' <= T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T <= 'XYZZY';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'XYZZY' <= T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 <= -10;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10 <= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0 <= -10;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10 <= 5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 <= -10.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10.0 <= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'foo' <= 'bar';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'bar' <= 'foo';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("120 <= '10';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("10 <= '120';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("120 <= '15';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("15 <= '120';", new EidosValue_Logical(false));
	EidosAssertScriptRaise("_Test(9) <= 5;", 9);
	EidosAssertScriptRaise("5 <= _Test(9);", 2);
	EidosAssertScriptSuccess("5 <= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10.0 <= -10.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 <= 5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0 <= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 <= '5';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'5' <= 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'foo' <= 'foo';", new EidosValue_Logical(true));
	EidosAssertScriptRaise("_Test(9) <= _Test(9);", 9);
	
	// operator ==
	#pragma mark operator ==
	EidosAssertScriptRaise("NULL==T;", 4);
	EidosAssertScriptRaise("NULL==0;", 4);
	EidosAssertScriptRaise("NULL==0.5;", 4);
	EidosAssertScriptRaise("NULL=='foo';", 4);
	EidosAssertScriptRaise("NULL==_Test(7);", 4);
	EidosAssertScriptRaise("NULL==(0:2);", 4);
	EidosAssertScriptRaise("T==NULL;", 1);
	EidosAssertScriptRaise("0==NULL;", 1);
	EidosAssertScriptRaise("0.5==NULL;", 3);
	EidosAssertScriptRaise("'foo'==NULL;", 5);
	EidosAssertScriptRaise("_Test(7)==NULL;", 8);
	EidosAssertScriptRaise("(0:2)==NULL;", 5);
	EidosAssertScriptRaise("==NULL;", 0);
	EidosAssertScriptSuccess("T == F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F == T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F == F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T == 0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == 1;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F == 0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F == 1;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == -5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-5 == T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 == T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == -5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-5.0 == T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 == T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == 'FOO';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'FOO' == T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T == 'XYZZY';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'XYZZY' == T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 == -10;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10 == 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0 == -10;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10 == 5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5 == -10.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("-10.0 == 5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'foo' == 'bar';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'bar' == 'foo';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("120 == '10';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("10 == '120';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("120 == '15';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("15 == '120';", new EidosValue_Logical(false));
	EidosAssertScriptRaise("_Test(9) == 5;", 9);
	EidosAssertScriptRaise("5 == _Test(9);", 2);
	EidosAssertScriptSuccess("5 == 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("-10.0 == -10.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 == 5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0 == 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5 == '5';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'5' == 5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'foo' == 'foo';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("_Test(9) == _Test(9);", new EidosValue_Logical(false));	// not the same object
	
	// operator !=
	#pragma mark operator !=
	EidosAssertScriptRaise("NULL!=T;", 4);
	EidosAssertScriptRaise("NULL!=0;", 4);
	EidosAssertScriptRaise("NULL!=0.5;", 4);
	EidosAssertScriptRaise("NULL!='foo';", 4);
	EidosAssertScriptRaise("NULL!=_Test(7);", 4);
	EidosAssertScriptRaise("NULL!=(0:2);", 4);
	EidosAssertScriptRaise("T!=NULL;", 1);
	EidosAssertScriptRaise("0!=NULL;", 1);
	EidosAssertScriptRaise("0.5!=NULL;", 3);
	EidosAssertScriptRaise("'foo'!=NULL;", 5);
	EidosAssertScriptRaise("_Test(7)!=NULL;", 8);
	EidosAssertScriptRaise("(0:2)!=NULL;", 5);
	EidosAssertScriptRaise("!=NULL;", 0);
	EidosAssertScriptSuccess("T != F;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != T;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("F != T;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("F != F;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("T != 0;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != 1;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("F != 0;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("F != 1;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != -5;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("-5 != T;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != 5;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("5 != T;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != -5.0;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("-5.0 != T;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != 5.0;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("5.0 != T;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != 'FOO';", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("'FOO' != T;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("T != 'XYZZY';", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("'XYZZY' != T;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("5 != -10;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("-10 != 5;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("5.0 != -10;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("-10 != 5.0;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("5 != -10.0;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("-10.0 != 5;", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("'foo' != 'bar';", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("'bar' != 'foo';", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("120 != '10';", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("10 != '120';", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("120 != '15';", new EidosValue_Logical(!false));
	EidosAssertScriptSuccess("15 != '120';", new EidosValue_Logical(!false));
	EidosAssertScriptRaise("_Test(9) != 5;", 9);
	EidosAssertScriptRaise("5 != _Test(9);", 2);
	EidosAssertScriptSuccess("5 != 5;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("-10.0 != -10.0;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("5 != 5.0;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("5.0 != 5;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("5 != '5';", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("'5' != 5;", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("'foo' != 'foo';", new EidosValue_Logical(!true));
	EidosAssertScriptSuccess("_Test(9) != _Test(9);", new EidosValue_Logical(!false));	// not the same object
	
	// operator :
	#pragma mark operator :
	EidosAssertScriptRaise("NULL:T;", 4);
	EidosAssertScriptRaise("NULL:0;", 4);
	EidosAssertScriptRaise("NULL:0.5;", 4);
	EidosAssertScriptRaise("NULL:'foo';", 4);
	EidosAssertScriptRaise("NULL:_Test(7);", 4);
	EidosAssertScriptRaise("NULL:(0:2);", 4);
	EidosAssertScriptRaise("T:NULL;", 1);
	EidosAssertScriptRaise("0:NULL;", 1);
	EidosAssertScriptRaise("0.5:NULL;", 3);
	EidosAssertScriptRaise("'foo':NULL;", 5);
	EidosAssertScriptRaise("_Test(7):NULL;", 8);
	EidosAssertScriptRaise("(0:2):NULL;", 5);
	EidosAssertScriptRaise(":NULL;", 0);
	EidosAssertScriptSuccess("1:5;", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("5:1;", new EidosValue_Int_vector(5, 4, 3, 2, 1));
	EidosAssertScriptSuccess("-2:1;", new EidosValue_Int_vector(-2, -1, 0, 1));
	EidosAssertScriptSuccess("1:-2;", new EidosValue_Int_vector(1, 0, -1, -2));
	EidosAssertScriptSuccess("1:1;", new EidosValue_Int_singleton_const(1));
	EidosAssertScriptSuccess("1.0:5;", new EidosValue_Float_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("5.0:1;", new EidosValue_Float_vector(5, 4, 3, 2, 1));
	EidosAssertScriptSuccess("-2.0:1;", new EidosValue_Float_vector(-2, -1, 0, 1));
	EidosAssertScriptSuccess("1.0:-2;", new EidosValue_Float_vector(1, 0, -1, -2));
	EidosAssertScriptSuccess("1.0:1;", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptSuccess("1:5.0;", new EidosValue_Float_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("5:1.0;", new EidosValue_Float_vector(5, 4, 3, 2, 1));
	EidosAssertScriptSuccess("-2:1.0;", new EidosValue_Float_vector(-2, -1, 0, 1));
	EidosAssertScriptSuccess("1:-2.0;", new EidosValue_Float_vector(1, 0, -1, -2));
	EidosAssertScriptSuccess("1:1.0;", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptRaise("1:F;", 1);
	EidosAssertScriptRaise("F:1;", 1);
	EidosAssertScriptRaise("T:F;", 1);
	EidosAssertScriptRaise("'a':'z';", 3);
	EidosAssertScriptRaise("1:(2:3);", 1);
	EidosAssertScriptRaise("(1:2):3;", 5);
	EidosAssertScriptSuccess("1.5:4.7;", new EidosValue_Float_vector(1.5, 2.5, 3.5, 4.5));
	EidosAssertScriptSuccess("1.5:-2.7;", new EidosValue_Float_vector(1.5, 0.5, -0.5, -1.5, -2.5));
	EidosAssertScriptRaise("1.5:INF;", 3);
	EidosAssertScriptRaise("1.5:NAN;", 3);
	EidosAssertScriptRaise("INF:1.5;", 3);
	EidosAssertScriptRaise("NAN:1.5;", 3);
	
	// operator ^
	#pragma mark operator ^
	EidosAssertScriptRaise("NULL^T;", 4);
	EidosAssertScriptRaise("NULL^0;", 4);
	EidosAssertScriptRaise("NULL^0.5;", 4);
	EidosAssertScriptRaise("NULL^'foo';", 4);
	EidosAssertScriptRaise("NULL^_Test(7);", 4);
	EidosAssertScriptRaise("NULL^(0:2);", 4);
	EidosAssertScriptRaise("T^NULL;", 1);
	EidosAssertScriptRaise("0^NULL;", 1);
	EidosAssertScriptRaise("0.5^NULL;", 3);
	EidosAssertScriptRaise("'foo'^NULL;", 5);
	EidosAssertScriptRaise("_Test(7)^NULL;", 8);
	EidosAssertScriptRaise("(0:2)^NULL;", 5);
	EidosAssertScriptRaise("^NULL;", 0);
	EidosAssertScriptSuccess("1^1;", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptSuccess("1^-1;", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptSuccess("(0:2)^10;", new EidosValue_Float_vector(0, 1, 1024));
	EidosAssertScriptSuccess("10^(0:2);", new EidosValue_Float_vector(1, 10, 100));
	EidosAssertScriptSuccess("(15:13)^(0:2);", new EidosValue_Float_vector(1, 14, 169));
	EidosAssertScriptRaise("(15:12)^(0:2);", 7);
	EidosAssertScriptRaise("NULL^(0:2);", 4);
	EidosAssertScriptSuccess("1^1.0;", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptSuccess("1.0^1;", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptSuccess("1.0^-1.0;", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptSuccess("(0:2.0)^10;", new EidosValue_Float_vector(0, 1, 1024));
	EidosAssertScriptSuccess("10.0^(0:2);", new EidosValue_Float_vector(1, 10, 100));
	EidosAssertScriptSuccess("(15.0:13)^(0:2.0);", new EidosValue_Float_vector(1, 14, 169));
	EidosAssertScriptRaise("(15:12.0)^(0:2);", 9);
	EidosAssertScriptRaise("NULL^(0:2.0);", 4);
	EidosAssertScriptRaise("'foo'^5;", 5);
	EidosAssertScriptRaise("T^F;", 1);
	EidosAssertScriptRaise("T^T;", 1);
	EidosAssertScriptRaise("F^F;", 1);
	EidosAssertScriptRaise("^5;", 0);
	EidosAssertScriptRaise("^5.0;", 0);
	EidosAssertScriptRaise("^'foo';", 0);
	EidosAssertScriptRaise("^T;", 0);
	EidosAssertScriptSuccess("4^(3^2);", new EidosValue_Float_singleton_const(262144));		// right-associative!
	EidosAssertScriptSuccess("4^3^2;", new EidosValue_Float_singleton_const(262144));		// right-associative!
	
	// operator &
	#pragma mark operator &
	EidosAssertScriptRaise("NULL&T;", 4);
	EidosAssertScriptRaise("NULL&0;", 4);
	EidosAssertScriptRaise("NULL&0.5;", 4);
	EidosAssertScriptRaise("NULL&'foo';", 4);
	EidosAssertScriptRaise("NULL&_Test(7);", 4);
	EidosAssertScriptRaise("NULL&(0:2);", 4);
	EidosAssertScriptRaise("T&NULL;", 1);
	EidosAssertScriptRaise("0&NULL;", 1);
	EidosAssertScriptRaise("0.5&NULL;", 3);
	EidosAssertScriptRaise("'foo'&NULL;", 5);
	EidosAssertScriptRaise("_Test(7)&NULL;", 8);
	EidosAssertScriptRaise("(0:2)&NULL;", 5);
	EidosAssertScriptRaise("&NULL;", 0);
	EidosAssertScriptSuccess("T&T&T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T&T&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T&F&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T&F&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&T&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&T&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&F&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&F&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) & F;", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("c(T,F,T,F) & T;", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("F & c(T,F,T,F);", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("T & c(T,F,T,F);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(T,T,F,F);", new EidosValue_Logical(true, false, false, false));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(F,F,T,T);", new EidosValue_Logical(false, false, true, false));
	EidosAssertScriptSuccess("c(T,T,F,F) & c(T,F,T,F);", new EidosValue_Logical(true, false, false, false));
	EidosAssertScriptSuccess("c(F,F,T,T) & c(T,F,T,F);", new EidosValue_Logical(false, false, true, false));
	EidosAssertScriptRaise("c(T,F,T,F) & c(F,F);", 11);
	EidosAssertScriptRaise("c(T,T) & c(T,F,T,F);", 7);
	EidosAssertScriptRaise("c(T,F,T,F) & _Test(3);", 11);
	EidosAssertScriptRaise("_Test(3) & c(T,F,T,F);", 9);
	EidosAssertScriptSuccess("5&T&T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T&5&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T&F&5;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5&F&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("0&T&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&T&0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&0&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&0&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) & 0;", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("c(7,0,5,0) & T;", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("F & c(5,0,7,0);", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("9 & c(T,F,T,F);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c(7,0,5,0) & c(T,T,F,F);", new EidosValue_Logical(true, false, false, false));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0,0,5,7);", new EidosValue_Logical(false, false, true, false));
	EidosAssertScriptSuccess("5.0&T&T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T&5.0&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T&F&5.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("5.0&F&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("0.0&T&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&T&0.0;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&0.0&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&0.0&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) & 0.0;", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & T;", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("F & c(5.0,0.0,7.0,0.0);", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("9.0 & c(T,F,T,F);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & c(T,T,F,F);", new EidosValue_Logical(true, false, false, false));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0.0,0.0,5.0,7.0);", new EidosValue_Logical(false, false, true, false));
	EidosAssertScriptSuccess("INF&T&T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T&INF&F;", new EidosValue_Logical(false));
	EidosAssertScriptRaise("T&NAN&F;", 1);
	EidosAssertScriptRaise("NAN&T&T;", 3);
	EidosAssertScriptSuccess("'foo'&T&T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T&'foo'&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("T&F&'foo';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("'foo'&F&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("''&T&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&T&'';", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&''&T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("F&''&F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) & '';", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("c('foo','','foo','') & T;", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("F & c('foo','','foo','');", new EidosValue_Logical(false, false, false, false));
	EidosAssertScriptSuccess("'foo' & c(T,F,T,F);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c('foo','','foo','') & c(T,T,F,F);", new EidosValue_Logical(true, false, false, false));
	EidosAssertScriptSuccess("c(T,F,T,F) & c('','','foo','foo');", new EidosValue_Logical(false, false, true, false));
	
	// operator |
	#pragma mark operator |
	EidosAssertScriptRaise("NULL|T;", 4);
	EidosAssertScriptRaise("NULL|0;", 4);
	EidosAssertScriptRaise("NULL|0.5;", 4);
	EidosAssertScriptRaise("NULL|'foo';", 4);
	EidosAssertScriptRaise("NULL|_Test(7);", 4);
	EidosAssertScriptRaise("NULL|(0:2);", 4);
	EidosAssertScriptRaise("T|NULL;", 1);
	EidosAssertScriptRaise("0|NULL;", 1);
	EidosAssertScriptRaise("0.5|NULL;", 3);
	EidosAssertScriptRaise("'foo'|NULL;", 5);
	EidosAssertScriptRaise("_Test(7)|NULL;", 8);
	EidosAssertScriptRaise("(0:2)|NULL;", 5);
	EidosAssertScriptRaise("|NULL;", 0);
	EidosAssertScriptSuccess("T|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|T|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|F|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|F|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|T|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|F|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|F|F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) | F;", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c(T,F,T,F) | T;", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("F | c(T,F,T,F);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("T | c(T,F,T,F);", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(T,T,F,F);", new EidosValue_Logical(true, true, true, false));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(F,F,T,T);", new EidosValue_Logical(true, false, true, true));
	EidosAssertScriptSuccess("c(T,T,F,F) | c(T,F,T,F);", new EidosValue_Logical(true, true, true, false));
	EidosAssertScriptSuccess("c(F,F,T,T) | c(T,F,T,F);", new EidosValue_Logical(true, false, true, true));
	EidosAssertScriptRaise("c(T,F,T,F) | c(F,F);", 11);
	EidosAssertScriptRaise("c(T,T) | c(T,F,T,F);", 7);
	EidosAssertScriptRaise("c(T,F,T,F) | _Test(3);", 11);
	EidosAssertScriptRaise("_Test(3) | c(T,F,T,F);", 9);
	EidosAssertScriptSuccess("5|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|5|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|F|5;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5|F|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("0|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|T|0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|0|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|0|F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) | 0;", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c(7,0,5,0) | T;", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("F | c(5,0,7,0);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("9 | c(T,F,T,F);", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("c(7,0,5,0) | c(T,T,F,F);", new EidosValue_Logical(true, true, true, false));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0,0,5,7);", new EidosValue_Logical(true, false, true, true));
	EidosAssertScriptSuccess("5.0|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|5.0|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|F|5.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("5.0|F|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("0.0|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|T|0.0;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|0.0|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|0.0|F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) | 0.0;", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | T;", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("F | c(5.0,0.0,7.0,0.0);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("9.0 | c(T,F,T,F);", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | c(T,T,F,F);", new EidosValue_Logical(true, true, true, false));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0.0,0.0,5.0,7.0);", new EidosValue_Logical(true, false, true, true));
	EidosAssertScriptSuccess("INF|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|INF|F;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("T|NAN|F;", 1);
	EidosAssertScriptRaise("NAN|T|T;", 3);
	EidosAssertScriptSuccess("'foo'|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|'foo'|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("T|F|'foo';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("'foo'|F|F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("''|T|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|T|'';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|''|T;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("F|''|F;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("c(T,F,T,F) | '';", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("c('foo','','foo','') | T;", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("F | c('foo','','foo','');", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("'foo' | c(T,F,T,F);", new EidosValue_Logical(true, true, true, true));
	EidosAssertScriptSuccess("c('foo','','foo','') | c(T,T,F,F);", new EidosValue_Logical(true, true, true, false));
	EidosAssertScriptSuccess("c(T,F,T,F) | c('','','foo','foo');", new EidosValue_Logical(true, false, true, true));
	
	// operator !
	#pragma mark operator !
	EidosAssertScriptRaise("!NULL;", 0);
	EidosAssertScriptSuccess("!T;", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("!F;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("!c(F,T,F,T);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("!c(0,5,0,1);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("!c(0,5.0,0,1.0);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptRaise("!c(0,NAN,0,1.0);", 0);
	EidosAssertScriptSuccess("!c(0,INF,0,1.0);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("!c('','foo','','bar');", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptRaise("!_Test(5);", 0);
	
	
	// ************************************************************************************
	//
	//	Keyword tests
	//
	
	// if
	#pragma mark if
	EidosAssertScriptSuccess("if (T) 23;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if (F) 23;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (6 > 5) 23;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if (6 < 5) 23;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (6 == (6:9)) 23;", 0);
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (_Test(6)) 23;", 0);
	EidosAssertScriptRaise("if (NULL) 23;", 0);
	
	// if-else
	#pragma mark if-else
	EidosAssertScriptSuccess("if (T) 23; else 42;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if (F) 23; else 42;", new EidosValue_Int_singleton_const(42));
	EidosAssertScriptSuccess("if (6 > 5) 23; else 42;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if (6 < 5) 23; else 42;", new EidosValue_Int_singleton_const(42));
	EidosAssertScriptRaise("if (6 == (6:9)) 23; else 42;", 0);
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23; else 42;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23; else 42;", new EidosValue_Int_singleton_const(42));
	EidosAssertScriptRaise("if (_Test(6)) 23; else 42;", 0);
	EidosAssertScriptRaise("if (NULL) 23; else 42;", 0);
	
	// do
	#pragma mark do
	EidosAssertScriptSuccess("x=1; do x=x*2; while (x<100); x;", new EidosValue_Int_singleton_const(128));
	EidosAssertScriptSuccess("x=200; do x=x*2; while (x<100); x;", new EidosValue_Int_singleton_const(400));
	EidosAssertScriptSuccess("x=1; do { x=x*2; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton_const(127));
	EidosAssertScriptSuccess("x=200; do { x=x*2; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton_const(401));
	EidosAssertScriptRaise("x=1; do x=x*2; while (x < 100:102); x;", 5);
	EidosAssertScriptRaise("x=200; do x=x*2; while (x < 100:102); x;", 7);
	EidosAssertScriptSuccess("x=1; do x=x*2; while ((x < 100:102)[0]); x;", new EidosValue_Int_singleton_const(128));
	EidosAssertScriptSuccess("x=200; do x=x*2; while ((x < 100:102)[0]); x;", new EidosValue_Int_singleton_const(400));
	EidosAssertScriptRaise("x=200; do x=x*2; while (_Test(6)); x;", 7);
	EidosAssertScriptRaise("x=200; do x=x*2; while (NULL); x;", 7);
	
	// while
	#pragma mark while
	EidosAssertScriptSuccess("x=1; while (x<100) x=x*2; x;", new EidosValue_Int_singleton_const(128));
	EidosAssertScriptSuccess("x=200; while (x<100) x=x*2; x;", new EidosValue_Int_singleton_const(200));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; x=x+1; } x;", new EidosValue_Int_singleton_const(127));
	EidosAssertScriptSuccess("x=200; while (x<100) { x=x*2; x=x+1; } x;", new EidosValue_Int_singleton_const(200));
	EidosAssertScriptRaise("x=1; while (x < 100:102) x=x*2; x;", 5);
	EidosAssertScriptRaise("x=200; while (x < 100:102) x=x*2; x;", 7);
	EidosAssertScriptSuccess("x=1; while ((x < 100:102)[0]) x=x*2; x;", new EidosValue_Int_singleton_const(128));
	EidosAssertScriptSuccess("x=200; while ((x < 100:102)[0]) x=x*2; x;", new EidosValue_Int_singleton_const(200));
	EidosAssertScriptRaise("x=200; while (_Test(6)) x=x*2; x;", 7);
	EidosAssertScriptRaise("x=200; while (NULL) x=x*2; x;", 7);
	
	// for and in
	#pragma mark for / in
	EidosAssertScriptSuccess("x=0; for (y in 33) x=x+y; x;", new EidosValue_Int_singleton_const(33));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) x=x+y; x;", new EidosValue_Int_singleton_const(55));
	EidosAssertScriptSuccess("x=0; for (y in 10:1) x=x+y; x;", new EidosValue_Int_singleton_const(55));
	EidosAssertScriptSuccess("x=0; for (y in 1.0:10) x=x+y; x;", new EidosValue_Float_singleton_const(55.0));
	EidosAssertScriptSuccess("x=0; for (y in c('foo', 'bar')) x=x+y; x;", new EidosValue_String("0foobar"));
	EidosAssertScriptSuccess("x=0; for (y in c(T,T,F,F,T,F)) x=x+asInteger(y); x;", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("x=0; for (y in _Test(7)) x=x+y._yolk; x;", new EidosValue_Int_singleton_const(7));
	EidosAssertScriptSuccess("x=0; for (y in rep(_Test(7),3)) x=x+y._yolk; x;", new EidosValue_Int_singleton_const(21));
	EidosAssertScriptRaise("x=0; y=0:2; for (y[0] in 2:4) x=x+sum(y); x;", 18);	// lvalue must be an identifier, at present
	EidosAssertScriptRaise("x=0; for (y in NULL) x;", 5);
	
	// next
	#pragma mark next
	EidosAssertScriptRaise("next;", 0);
	EidosAssertScriptRaise("if (T) next;", 7);
	EidosAssertScriptSuccess("if (F) next;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (T) next; else 42;", 7);
	EidosAssertScriptSuccess("if (F) next; else 42;", new EidosValue_Int_singleton_const(42));
	EidosAssertScriptSuccess("if (T) 23; else next;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptRaise("if (F) 23; else next;", 16);
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) next; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton_const(124));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) next; x=x+1; } x;", new EidosValue_Int_singleton_const(124));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) next; x=x+y; } x;", new EidosValue_Int_singleton_const(50));
	
	// break
	#pragma mark break
	EidosAssertScriptRaise("break;", 0);
	EidosAssertScriptRaise("if (T) break;", 7);
	EidosAssertScriptSuccess("if (F) break;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (T) break; else 42;", 7);
	EidosAssertScriptSuccess("if (F) break; else 42;", new EidosValue_Int_singleton_const(42));
	EidosAssertScriptSuccess("if (T) 23; else break;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptRaise("if (F) 23; else break;", 16);
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) break; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton_const(62));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) break; x=x+1; } x;", new EidosValue_Int_singleton_const(62));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) break; x=x+y; } x;", new EidosValue_Int_singleton_const(10));
	
	// return
	#pragma mark return
	EidosAssertScriptSuccess("return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("return -13;", new EidosValue_Int_singleton_const(-13));
	EidosAssertScriptSuccess("if (T) return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13;", new EidosValue_Int_singleton_const(-13));
	EidosAssertScriptSuccess("if (F) return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) return -13;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return; else 42;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13; else 42;", new EidosValue_Int_singleton_const(-13));
	EidosAssertScriptSuccess("if (F) return; else 42;", new EidosValue_Int_singleton_const(42));
	EidosAssertScriptSuccess("if (F) return -13; else 42;", new EidosValue_Int_singleton_const(42));
	EidosAssertScriptSuccess("if (T) 23; else return;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if (T) 23; else return -13;", new EidosValue_Int_singleton_const(23));
	EidosAssertScriptSuccess("if (F) 23; else return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) 23; else return -13;", new EidosValue_Int_singleton_const(-13));
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return; x=x+1; } while (x<100); x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return x-5; x=x+1; } while (x<100); x;", new EidosValue_Int_singleton_const(57));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return; x=x+1; } x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return x-5; x=x+1; } x;", new EidosValue_Int_singleton_const(57));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return; x=x+y; } x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return x-5; x=x+y; } x;", new EidosValue_Int_singleton_const(5));
	
	
	// ************************************************************************************
	//
	//	Function tests
	//
	#pragma mark -
	#pragma mark Functions
	#pragma mark -
	
	#pragma mark math
	
	// abs()
	EidosAssertScriptSuccess("abs(5);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("abs(-5);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("abs(c(-2, 7, -18, 12));", new EidosValue_Int_vector(2, 7, 18, 12));
	EidosAssertScriptSuccess("abs(5.5);", new EidosValue_Float_singleton_const(5.5));
	EidosAssertScriptSuccess("abs(-5.5);", new EidosValue_Float_singleton_const(5.5));
	EidosAssertScriptSuccess("abs(c(-2.0, 7.0, -18.0, 12.0));", new EidosValue_Float_vector(2, 7, 18, 12));
	EidosAssertScriptRaise("abs(T);", 0);
	EidosAssertScriptRaise("abs('foo');", 0);
	EidosAssertScriptRaise("abs(_Test(7));", 0);
	EidosAssertScriptRaise("abs(NULL);", 0);
	EidosAssertScriptRaise("abs(logical(0));", 0);
	EidosAssertScriptSuccess("abs(integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("abs(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("abs(string(0));", 0);
	
	// acos()
	EidosAssertScriptSuccess("abs(acos(0) - PI/2) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(acos(1) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(acos(c(0, 1, -1)) - c(PI/2, 0, PI))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(acos(0.0) - PI/2) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(acos(1.0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(acos(c(0.0, 1.0, -1.0)) - c(PI/2, 0, PI))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("acos(T);", 0);
	EidosAssertScriptRaise("acos('foo');", 0);
	EidosAssertScriptRaise("acos(_Test(7));", 0);
	EidosAssertScriptRaise("acos(NULL);", 0);
	EidosAssertScriptRaise("acos(logical(0));", 0);
	EidosAssertScriptSuccess("acos(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("acos(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("acos(string(0));", 0);
	
	// asin()
	EidosAssertScriptSuccess("abs(asin(0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(asin(1) - PI/2) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(asin(c(0, 1, -1)) - c(0, PI/2, -PI/2))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(asin(0.0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(asin(1.0) - PI/2) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(asin(c(0.0, 1.0, -1.0)) - c(0, PI/2, -PI/2))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("asin(T);", 0);
	EidosAssertScriptRaise("asin('foo');", 0);
	EidosAssertScriptRaise("asin(_Test(7));", 0);
	EidosAssertScriptRaise("asin(NULL);", 0);
	EidosAssertScriptRaise("asin(logical(0));", 0);
	EidosAssertScriptSuccess("asin(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("asin(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("asin(string(0));", 0);
	
	// atan()
	EidosAssertScriptSuccess("abs(atan(0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(atan(1) - PI/4) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(atan(c(0, 1, -1)) - c(0, PI/4, -PI/4))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(atan(0.0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(atan(1.0) - PI/4) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(atan(c(0.0, 1.0, -1.0)) - c(0, PI/4, -PI/4))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("atan(T);", 0);
	EidosAssertScriptRaise("atan('foo');", 0);
	EidosAssertScriptRaise("atan(_Test(7));", 0);
	EidosAssertScriptRaise("atan(NULL);", 0);
	EidosAssertScriptRaise("atan(logical(0));", 0);
	EidosAssertScriptSuccess("atan(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("atan(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("atan(string(0));", 0);
	
	// atan2()
	EidosAssertScriptSuccess("abs(atan2(0, 1) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(atan2(0, -1) - PI) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(atan2(c(0, 0, -1), c(1, -1, 0)) - c(0, PI, -PI/2))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(atan2(0.0, 1.0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(atan2(0.0, -1.0) - PI) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(atan2(c(0.0, 0.0, -1.0), c(1.0, -1.0, 0.0)) - c(0, PI, -PI/2))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("atan2(T);", 0);
	EidosAssertScriptRaise("atan2('foo');", 0);
	EidosAssertScriptRaise("atan2(_Test(7));", 0);
	EidosAssertScriptRaise("atan2(NULL);", 0);
	EidosAssertScriptRaise("atan2(logical(0), logical(0));", 0);
	EidosAssertScriptSuccess("atan2(integer(0), integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("atan2(float(0), float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("atan2(string(0), string(0));", 0);
	EidosAssertScriptRaise("atan2(0.0, c(0.0, 1.0));", 0);		// argument count mismatch
	
	// ceil()
	EidosAssertScriptSuccess("ceil(5.1);", new EidosValue_Float_singleton_const(6.0));
	EidosAssertScriptSuccess("ceil(-5.1);", new EidosValue_Float_singleton_const(-5.0));
	EidosAssertScriptSuccess("ceil(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector(-2.0, 8, -18, 13));
	EidosAssertScriptRaise("ceil(T);", 0);
	EidosAssertScriptRaise("ceil(5);", 0);
	EidosAssertScriptRaise("ceil('foo');", 0);
	EidosAssertScriptRaise("ceil(_Test(7));", 0);
	EidosAssertScriptRaise("ceil(NULL);", 0);
	EidosAssertScriptRaise("ceil(logical(0));", 0);
	EidosAssertScriptRaise("ceil(integer(0));", 0);
	EidosAssertScriptSuccess("ceil(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("ceil(string(0));", 0);
	
	// cos()
	EidosAssertScriptSuccess("abs(cos(0) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(cos(0.0) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(cos(PI/2) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(cos(c(0, PI/2, PI)) - c(1, 0, -1))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("cos(T);", 0);
	EidosAssertScriptRaise("cos('foo');", 0);
	EidosAssertScriptRaise("cos(_Test(7));", 0);
	EidosAssertScriptRaise("cos(NULL);", 0);
	EidosAssertScriptRaise("cos(logical(0));", 0);
	EidosAssertScriptSuccess("cos(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("cos(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("cos(string(0));", 0);
	
	// exp()
	EidosAssertScriptSuccess("abs(exp(0) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(exp(0.0) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(exp(1.0) - E) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(exp(c(0, 1.0, -1)) - c(1, E, 0.3678794))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("exp(T);", 0);
	EidosAssertScriptRaise("exp('foo');", 0);
	EidosAssertScriptRaise("exp(_Test(7));", 0);
	EidosAssertScriptRaise("exp(NULL);", 0);
	EidosAssertScriptRaise("exp(logical(0));", 0);
	EidosAssertScriptSuccess("exp(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("exp(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("exp(string(0));", 0);
	
	// floor()
	EidosAssertScriptSuccess("floor(5.1);", new EidosValue_Float_singleton_const(5.0));
	EidosAssertScriptSuccess("floor(-5.1);", new EidosValue_Float_singleton_const(-6.0));
	EidosAssertScriptSuccess("floor(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector(-3.0, 7, -19, 12));
	EidosAssertScriptRaise("floor(T);", 0);
	EidosAssertScriptRaise("floor(5);", 0);
	EidosAssertScriptRaise("floor('foo');", 0);
	EidosAssertScriptRaise("floor(_Test(7));", 0);
	EidosAssertScriptRaise("floor(NULL);", 0);
	EidosAssertScriptRaise("floor(logical(0));", 0);
	EidosAssertScriptRaise("floor(integer(0));", 0);
	EidosAssertScriptSuccess("floor(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("floor(string(0));", 0);
	
	// isFinite()
	EidosAssertScriptSuccess("isFinite(0.0);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isFinite(0.05);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isFinite(INF);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isFinite(NAN);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isFinite(c(5/0, 0/0, 17.0));", new EidosValue_Logical(false, false, true));	// INF, NAN, normal
	EidosAssertScriptRaise("isFinite(1);", 0);
	EidosAssertScriptRaise("isFinite(T);", 0);
	EidosAssertScriptRaise("isFinite('foo');", 0);
	EidosAssertScriptRaise("isFinite(_Test(7));", 0);
	EidosAssertScriptRaise("isFinite(NULL);", 0);
	EidosAssertScriptRaise("isFinite(logical(0));", 0);
	EidosAssertScriptRaise("isFinite(integer(0));", 0);
	EidosAssertScriptSuccess("isFinite(float(0));", new EidosValue_Logical());
	EidosAssertScriptRaise("isFinite(string(0));", 0);
	
	// isInfinite()
	EidosAssertScriptSuccess("isInfinite(0.0);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInfinite(0.05);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInfinite(INF);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isInfinite(NAN);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInfinite(c(5/0, 0/0, 17.0));", new EidosValue_Logical(true, false, false));	// INF, NAN, normal
	EidosAssertScriptRaise("isInfinite(1);", 0);
	EidosAssertScriptRaise("isInfinite(T);", 0);
	EidosAssertScriptRaise("isInfinite('foo');", 0);
	EidosAssertScriptRaise("isInfinite(_Test(7));", 0);
	EidosAssertScriptRaise("isInfinite(NULL);", 0);
	EidosAssertScriptRaise("isInfinite(logical(0));", 0);
	EidosAssertScriptRaise("isInfinite(integer(0));", 0);
	EidosAssertScriptSuccess("isInfinite(float(0));", new EidosValue_Logical());
	EidosAssertScriptRaise("isInfinite(string(0));", 0);
	
	// isNAN()
	EidosAssertScriptSuccess("isNAN(0.0);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNAN(0.05);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNAN(INF);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNAN(NAN);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isNAN(c(5/0, 0/0, 17.0));", new EidosValue_Logical(false, true, false));	// INF, NAN, normal
	EidosAssertScriptRaise("isNAN(1);", 0);
	EidosAssertScriptRaise("isNAN(T);", 0);
	EidosAssertScriptRaise("isNAN('foo');", 0);
	EidosAssertScriptRaise("isNAN(_Test(7));", 0);
	EidosAssertScriptRaise("isNAN(NULL);", 0);
	EidosAssertScriptRaise("isNAN(logical(0));", 0);
	EidosAssertScriptRaise("isNAN(integer(0));", 0);
	EidosAssertScriptSuccess("isNAN(float(0));", new EidosValue_Logical());
	EidosAssertScriptRaise("isNAN(string(0));", 0);
	
	// log()
	EidosAssertScriptSuccess("abs(log(1) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(log(E) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(log(E^3.5) - 3.5) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(log(c(1, E, E^3.5)) - c(0, 1, 3.5))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("log(T);", 0);
	EidosAssertScriptRaise("log('foo');", 0);
	EidosAssertScriptRaise("log(_Test(7));", 0);
	EidosAssertScriptRaise("log(NULL);", 0);
	EidosAssertScriptRaise("log(logical(0));", 0);
	EidosAssertScriptSuccess("log(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("log(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("log(string(0));", 0);
	
	// log10()
	EidosAssertScriptSuccess("abs(log10(1) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(log10(10) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(log10(0.001) - -3) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(log10(c(1, 10, 0.001)) - c(0, 1, -3))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("log10(T);", 0);
	EidosAssertScriptRaise("log10('foo');", 0);
	EidosAssertScriptRaise("log10(_Test(7));", 0);
	EidosAssertScriptRaise("log10(NULL);", 0);
	EidosAssertScriptRaise("log10(logical(0));", 0);
	EidosAssertScriptSuccess("log10(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("log10(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("log10(string(0));", 0);
	
	// log2()
	EidosAssertScriptSuccess("abs(log2(1) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(log2(2) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(log2(0.125) - -3) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(log2(c(1, 2, 0.125)) - c(0, 1, -3))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("log2(T);", 0);
	EidosAssertScriptRaise("log2('foo');", 0);
	EidosAssertScriptRaise("log2(_Test(7));", 0);
	EidosAssertScriptRaise("log2(NULL);", 0);
	EidosAssertScriptRaise("log2(logical(0));", 0);
	EidosAssertScriptSuccess("log2(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("log2(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("log2(string(0));", 0);
	
	// product()
	EidosAssertScriptSuccess("product(5);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("product(-5);", new EidosValue_Int_singleton_const(-5));
	EidosAssertScriptSuccess("product(c(-2, 7, -18, 12));", new EidosValue_Int_singleton_const(3024));
	EidosAssertScriptSuccess("product(c(200000000, 3000000000000, 1000));", new EidosValue_Float_singleton_const(6e23));
	EidosAssertScriptSuccess("product(5.5);", new EidosValue_Float_singleton_const(5.5));
	EidosAssertScriptSuccess("product(-5.5);", new EidosValue_Float_singleton_const(-5.5));
	EidosAssertScriptSuccess("product(c(-2.5, 7.5, -18.5, 12.5));", new EidosValue_Float_singleton_const(-2.5*7.5*-18.5*12.5));
	EidosAssertScriptRaise("product(T);", 0);
	EidosAssertScriptRaise("product('foo');", 0);
	EidosAssertScriptRaise("product(_Test(7));", 0);
	EidosAssertScriptRaise("product(NULL);", 0);
	EidosAssertScriptRaise("product(logical(0));", 0);
	EidosAssertScriptSuccess("product(integer(0));", new EidosValue_Int_singleton_const(1));	// product of no elements is 1 (as in R)
	EidosAssertScriptSuccess("product(float(0));", new EidosValue_Float_singleton_const(1.0));
	EidosAssertScriptRaise("product(string(0));", 0);
	
	// round()
	EidosAssertScriptSuccess("round(5.1);", new EidosValue_Float_singleton_const(5.0));
	EidosAssertScriptSuccess("round(-5.1);", new EidosValue_Float_singleton_const(-5.0));
	EidosAssertScriptSuccess("round(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector(-2.0, 7, -19, 13));
	EidosAssertScriptRaise("round(T);", 0);
	EidosAssertScriptRaise("round(5);", 0);
	EidosAssertScriptRaise("round('foo');", 0);
	EidosAssertScriptRaise("round(_Test(7));", 0);
	EidosAssertScriptRaise("round(NULL);", 0);
	EidosAssertScriptRaise("round(logical(0));", 0);
	EidosAssertScriptRaise("round(integer(0));", 0);
	EidosAssertScriptSuccess("round(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("round(string(0));", 0);
	
	// sin()
	EidosAssertScriptSuccess("abs(sin(0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(sin(0.0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(sin(PI/2) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(sin(c(0, PI/2, PI)) - c(0, 1, 0))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("sin(T);", 0);
	EidosAssertScriptRaise("sin('foo');", 0);
	EidosAssertScriptRaise("sin(_Test(7));", 0);
	EidosAssertScriptRaise("sin(NULL);", 0);
	EidosAssertScriptRaise("sin(logical(0));", 0);
	EidosAssertScriptSuccess("sin(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("sin(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("sin(string(0));", 0);
	
	// sqrt()
	EidosAssertScriptSuccess("sqrt(64);", new EidosValue_Float_singleton_const(8));
	EidosAssertScriptSuccess("isNAN(sqrt(-64));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sqrt(c(4, -16, 9, 1024));", new EidosValue_Float_vector(2, NAN, 3, 32));
	EidosAssertScriptSuccess("sqrt(64.0);", new EidosValue_Float_singleton_const(8));
	EidosAssertScriptSuccess("isNAN(sqrt(-64.0));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sqrt(c(4.0, -16.0, 9.0, 1024.0));", new EidosValue_Float_vector(2, NAN, 3, 32));
	EidosAssertScriptRaise("sqrt(T);", 0);
	EidosAssertScriptRaise("sqrt('foo');", 0);
	EidosAssertScriptRaise("sqrt(_Test(7));", 0);
	EidosAssertScriptRaise("sqrt(NULL);", 0);
	EidosAssertScriptRaise("sqrt(logical(0));", 0);
	EidosAssertScriptSuccess("sqrt(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("sqrt(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("sqrt(string(0));", 0);
	
	// sum()
	EidosAssertScriptSuccess("sum(5);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("sum(-5);", new EidosValue_Int_singleton_const(-5));
	EidosAssertScriptSuccess("sum(c(-2, 7, -18, 12));", new EidosValue_Int_singleton_const(-1));
	EidosAssertScriptSuccess("sum(c(200000000, 3000000000000));", new EidosValue_Int_singleton_const(3000200000000));
	EidosAssertScriptSuccess("sum(rep(3000000000000000000, 100));", new EidosValue_Float_singleton_const(3e20));
	EidosAssertScriptSuccess("sum(5.5);", new EidosValue_Float_singleton_const(5.5));
	EidosAssertScriptSuccess("sum(-5.5);", new EidosValue_Float_singleton_const(-5.5));
	EidosAssertScriptSuccess("sum(c(-2.5, 7.5, -18.5, 12.5));", new EidosValue_Float_singleton_const(-1));
	EidosAssertScriptSuccess("sum(T);", new EidosValue_Int_singleton_const(1));
	EidosAssertScriptSuccess("sum(c(T,F,T,F,T,T,T,F));", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptRaise("sum('foo');", 0);
	EidosAssertScriptRaise("sum(_Test(7));", 0);
	EidosAssertScriptRaise("sum(NULL);", 0);
	EidosAssertScriptSuccess("sum(logical(0));", new EidosValue_Int_singleton_const(0));	// sum of no elements is 0 (as in R)
	EidosAssertScriptSuccess("sum(integer(0));", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("sum(float(0));", new EidosValue_Float_singleton_const(0.0));
	EidosAssertScriptRaise("sum(string(0));", 0);
	
	// tan()
	EidosAssertScriptSuccess("abs(tan(0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(tan(0.0) - 0) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("abs(tan(PI/4) - 1) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(abs(tan(c(0, PI/4, -PI/4)) - c(0, 1, -1))) < 0.000001;", new EidosValue_Logical(true));
	EidosAssertScriptRaise("tan(T);", 0);
	EidosAssertScriptRaise("tan('foo');", 0);
	EidosAssertScriptRaise("tan(_Test(7));", 0);
	EidosAssertScriptRaise("tan(NULL);", 0);
	EidosAssertScriptRaise("tan(logical(0));", 0);
	EidosAssertScriptSuccess("tan(integer(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("tan(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("tan(string(0));", 0);
	
	// trunc()
	EidosAssertScriptSuccess("trunc(5.1);", new EidosValue_Float_singleton_const(5.0));
	EidosAssertScriptSuccess("trunc(-5.1);", new EidosValue_Float_singleton_const(-5.0));
	EidosAssertScriptSuccess("trunc(c(-2.1, 7.1, -18.8, 12.8));", new EidosValue_Float_vector(-2.0, 7, -18, 12));
	EidosAssertScriptRaise("trunc(T);", 0);
	EidosAssertScriptRaise("trunc(5);", 0);
	EidosAssertScriptRaise("trunc('foo');", 0);
	EidosAssertScriptRaise("trunc(_Test(7));", 0);
	EidosAssertScriptRaise("trunc(NULL);", 0);
	EidosAssertScriptRaise("trunc(logical(0));", 0);
	EidosAssertScriptRaise("trunc(integer(0));", 0);
	EidosAssertScriptSuccess("trunc(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptRaise("trunc(string(0));", 0);
	
	#pragma mark summary statistics
	
	// max()
	EidosAssertScriptSuccess("max(T);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("max(3);", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("max(3.5);", new EidosValue_Float_singleton_const(3.5));
	EidosAssertScriptSuccess("max('foo');", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("max(c(F, F, T, F, T));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("max(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton_const(19));
	EidosAssertScriptSuccess("max(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Float_singleton_const(19.1));
	EidosAssertScriptSuccess("max(c('foo', 'bar', 'baz'));", new EidosValue_String("foo"));
	EidosAssertScriptRaise("max(_Test(7));", 0);
	EidosAssertScriptSuccess("max(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(string(0));", gStaticEidosValueNULL);
	
	// mean()
	EidosAssertScriptRaise("mean(T);", 0);
	EidosAssertScriptSuccess("mean(3);", new EidosValue_Float_singleton_const(3));
	EidosAssertScriptSuccess("mean(3.5);", new EidosValue_Float_singleton_const(3.5));
	EidosAssertScriptRaise("mean('foo');", 0);
	EidosAssertScriptRaise("mean(c(F, F, T, F, T));", 0);
	EidosAssertScriptSuccess("mean(c(3, 7, 19, -5, 16));", new EidosValue_Float_singleton_const(8));
	EidosAssertScriptSuccess("mean(c(3.3, 7.2, 19.1, -5.6, 16.0));", new EidosValue_Float_singleton_const(8.0));
	EidosAssertScriptRaise("mean(c('foo', 'bar', 'baz'));", 0);
	EidosAssertScriptRaise("mean(_Test(7));", 0);
	EidosAssertScriptRaise("mean(NULL);", 0);
	EidosAssertScriptRaise("mean(logical(0));", 0);
	EidosAssertScriptSuccess("mean(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("mean(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("mean(string(0));", 0);
	
	// min()
	EidosAssertScriptSuccess("min(T);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("min(3);", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("min(3.5);", new EidosValue_Float_singleton_const(3.5));
	EidosAssertScriptSuccess("min('foo');", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("min(c(F, F, T, F, T));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("min(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton_const(-5));
	EidosAssertScriptSuccess("min(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Float_singleton_const(-5.8));
	EidosAssertScriptSuccess("min(c('foo', 'bar', 'baz'));", new EidosValue_String("bar"));
	EidosAssertScriptRaise("min(_Test(7));", 0);
	EidosAssertScriptSuccess("min(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(string(0));", gStaticEidosValueNULL);
	
	// range()
	EidosAssertScriptRaise("range(T);", 0);
	EidosAssertScriptSuccess("range(3);", new EidosValue_Int_vector(3, 3));
	EidosAssertScriptSuccess("range(3.5);", new EidosValue_Float_vector(3.5, 3.5));
	EidosAssertScriptRaise("range('foo');", 0);
	EidosAssertScriptRaise("range(c(F, F, T, F, T));", 0);
	EidosAssertScriptSuccess("range(c(3, 7, 19, -5, 9));", new EidosValue_Int_vector(-5, 19));
	EidosAssertScriptSuccess("range(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Float_vector(-5.8, 19.1));
	EidosAssertScriptRaise("range(c('foo', 'bar', 'baz'));", 0);
	EidosAssertScriptRaise("range(_Test(7));", 0);
	EidosAssertScriptRaise("range(NULL);", 0);
	EidosAssertScriptRaise("range(logical(0));", 0);
	EidosAssertScriptSuccess("range(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("range(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("range(string(0));", 0);
	
	// sd()
	EidosAssertScriptRaise("sd(T);", 0);
	EidosAssertScriptSuccess("sd(3);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(3.5);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd('foo');", 0);
	EidosAssertScriptRaise("sd(c(F, F, T, F, T));", 0);
	EidosAssertScriptSuccess("sd(c(2, 3, 2, 8, 0));", new EidosValue_Float_singleton_const(3));
	EidosAssertScriptSuccess("sd(c(9.1, 5.1, 5.1, 4.1, 7.1));", new EidosValue_Float_singleton_const(2.0));
	EidosAssertScriptRaise("sd(c('foo', 'bar', 'baz'));", 0);
	EidosAssertScriptRaise("sd(_Test(7));", 0);
	EidosAssertScriptRaise("sd(NULL);", 0);
	EidosAssertScriptRaise("sd(logical(0));", 0);
	EidosAssertScriptSuccess("sd(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd(string(0));", 0);
	
	#pragma mark vector construction
	
	// c()
	EidosAssertScriptSuccess("c();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("c(NULL);", 0);
	EidosAssertScriptSuccess("c(T);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("c(3);", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("c(3.1);", new EidosValue_Float_singleton_const(3.1));
	EidosAssertScriptSuccess("c('foo');", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("c(_Test(7))._yolk;", new EidosValue_Int_singleton_const(7));
	EidosAssertScriptRaise("c(NULL, NULL);", 0);
	EidosAssertScriptSuccess("c(T, F, T, T, T, F);", new EidosValue_Logical(true, false, true, true, true, false));
	EidosAssertScriptSuccess("c(3, 7, 19, -5, 9);", new EidosValue_Int_vector(3, 7, 19, -5, 9));
	EidosAssertScriptSuccess("c(3.3, 7.7, 19.1, -5.8, 9.0);", new EidosValue_Float_vector(3.3, 7.7, 19.1, -5.8, 9.0));
	EidosAssertScriptSuccess("c('foo', 'bar', 'baz');", new EidosValue_String("foo", "bar", "baz"));
	EidosAssertScriptSuccess("c(_Test(7), _Test(3), _Test(-9))._yolk;", new EidosValue_Int_vector(7, 3, -9));
	EidosAssertScriptSuccess("c(T, c(T, F, F), T, F);", new EidosValue_Logical(true, true, false, false, true, false));
	EidosAssertScriptSuccess("c(3, 7, c(17, -2), -5, 9);", new EidosValue_Int_vector(3, 7, 17, -2, -5, 9));
	EidosAssertScriptSuccess("c(3.3, 7.7, c(17.1, -2.9), -5.8, 9.0);", new EidosValue_Float_vector(3.3, 7.7, 17.1, -2.9, -5.8, 9.0));
	EidosAssertScriptSuccess("c('foo', c('bar', 'bar2', 'bar3'), 'baz');", new EidosValue_String("foo", "bar", "bar2", "bar3", "baz"));
	EidosAssertScriptSuccess("c(T, 3, F, 7);", new EidosValue_Int_vector(1, 3, 0, 7));
	EidosAssertScriptSuccess("c(T, 3, F, 7.1);", new EidosValue_Float_vector(1, 3, 0, 7.1));
	EidosAssertScriptSuccess("c(T, 3, 'bar', 7.1);", new EidosValue_String("T", "3", "bar", "7.1"));
	EidosAssertScriptRaise("c(T, NULL);", 0);
	EidosAssertScriptRaise("c(3, NULL);", 0);
	EidosAssertScriptRaise("c(3.1, NULL);", 0);
	EidosAssertScriptRaise("c('foo', NULL);", 0);
	EidosAssertScriptRaise("c(_Test(7), NULL);", 0);
	EidosAssertScriptRaise("c(NULL, T);", 0);
	EidosAssertScriptRaise("c(NULL, 3);", 0);
	EidosAssertScriptRaise("c(NULL, 3.1);", 0);
	EidosAssertScriptRaise("c(NULL, 'foo');", 0);
	EidosAssertScriptRaise("c(NULL, _Test(7));", 0);
	EidosAssertScriptRaise("c(T, _Test(7));", 0);
	EidosAssertScriptRaise("c(3, _Test(7));", 0);
	EidosAssertScriptRaise("c(3.1, _Test(7));", 0);
	EidosAssertScriptRaise("c('foo', _Test(7));", 0);
	
	// float()
	EidosAssertScriptSuccess("float(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("float(1);", new EidosValue_Float_singleton_const(0.0));
	EidosAssertScriptSuccess("float(2);", new EidosValue_Float_vector(0.0, 0.0));
	EidosAssertScriptSuccess("float(5);", new EidosValue_Float_vector(0.0, 0.0, 0.0, 0.0, 0.0));
	EidosAssertScriptRaise("float(-1);", 0);
	EidosAssertScriptRaise("float(-10000);", 0);
	EidosAssertScriptRaise("float(NULL);", 0);
	EidosAssertScriptRaise("float(integer(0));", 0);
	
	// integer()
	EidosAssertScriptSuccess("integer(0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("integer(1);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("integer(2);", new EidosValue_Int_vector(0, 0));
	EidosAssertScriptSuccess("integer(5);", new EidosValue_Int_vector(0, 0, 0, 0, 0));
	EidosAssertScriptRaise("integer(-1);", 0);
	EidosAssertScriptRaise("integer(-10000);", 0);
	EidosAssertScriptRaise("integer(NULL);", 0);
	EidosAssertScriptRaise("integer(integer(0));", 0);
	
	// logical()
	EidosAssertScriptSuccess("logical(0);", new EidosValue_Logical());
	EidosAssertScriptSuccess("logical(1);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("logical(2);", new EidosValue_Logical(false, false));
	EidosAssertScriptSuccess("logical(5);", new EidosValue_Logical(false, false, false, false, false));
	EidosAssertScriptRaise("logical(-1);", 0);
	EidosAssertScriptRaise("logical(-10000);", 0);
	EidosAssertScriptRaise("logical(NULL);", 0);
	EidosAssertScriptRaise("logical(integer(0));", 0);
	
	// object()
	EidosAssertScriptSuccess("object();", new EidosValue_Object_vector());
	EidosAssertScriptRaise("object(NULL);", 0);
	EidosAssertScriptRaise("object(integer(0));", 0);
	
	// rbinom()
	EidosAssertScriptSuccess("rbinom(0, 10, 0.5);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("rbinom(3, 10, 0.0);", new EidosValue_Int_vector(0, 0, 0));
	EidosAssertScriptSuccess("rbinom(3, 10, 1.0);", new EidosValue_Int_vector(10, 10, 10));
	EidosAssertScriptSuccess("rbinom(3, 0, 0.0);", new EidosValue_Int_vector(0, 0, 0));
	EidosAssertScriptSuccess("rbinom(3, 0, 1.0);", new EidosValue_Int_vector(0, 0, 0));
	EidosAssertScriptSuccess("setSeed(1); rbinom(5, 10, 0.5);", new EidosValue_Int_vector(4, 8, 5, 3, 4));
	EidosAssertScriptSuccess("setSeed(2); rbinom(5, 10, 0.5);", new EidosValue_Int_vector(7, 6, 3, 6, 3));
	EidosAssertScriptSuccess("setSeed(3); rbinom(5, 1000, 0.01);", new EidosValue_Int_vector(11, 16, 10, 14, 10));
	EidosAssertScriptSuccess("setSeed(4); rbinom(5, 1000, 0.99);", new EidosValue_Int_vector(992, 990, 995, 991, 995));
	EidosAssertScriptSuccess("setSeed(5); rbinom(3, 100, c(0.1, 0.5, 0.9));", new EidosValue_Int_vector(7, 50, 87));
	EidosAssertScriptSuccess("setSeed(6); rbinom(3, c(10, 30, 50), 0.5);", new EidosValue_Int_vector(6, 12, 26));
	EidosAssertScriptRaise("rbinom(-1, 10, 0.5);", 0);
	EidosAssertScriptRaise("rbinom(3, -1, 0.5);", 0);
	EidosAssertScriptRaise("rbinom(3, 10, -0.1);", 0);
	EidosAssertScriptRaise("rbinom(3, 10, 1.1);", 0);
	EidosAssertScriptRaise("rbinom(3, 10, c(0.1, 0.2));", 0);
	EidosAssertScriptRaise("rbinom(3, c(10, 12), 0.5);", 0);
	
	// rep()
	EidosAssertScriptRaise("rep(NULL, -1);", 0);
	EidosAssertScriptRaise("rep(T, -1);", 0);
	EidosAssertScriptRaise("rep(3, -1);", 0);
	EidosAssertScriptRaise("rep(3.5, -1);", 0);
	EidosAssertScriptRaise("rep('foo', -1);", 0);
	EidosAssertScriptRaise("rep(_Test(7), -1);", 0);
	EidosAssertScriptSuccess("rep(NULL, 0);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("rep(T, 0);", new EidosValue_Logical());
	EidosAssertScriptSuccess("rep(3, 0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("rep(3.5, 0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("rep('foo', 0);", new EidosValue_String());
	EidosAssertScriptSuccess("rep(_Test(7), 0);", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("rep(NULL, 2);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("rep(T, 2);", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("rep(3, 2);", new EidosValue_Int_vector(3, 3));
	EidosAssertScriptSuccess("rep(3.5, 2);", new EidosValue_Float_vector(3.5, 3.5));
	EidosAssertScriptSuccess("rep('foo', 2);", new EidosValue_String("foo", "foo"));
	EidosAssertScriptSuccess("rep(_Test(7), 2)._yolk;", new EidosValue_Int_vector(7, 7));
	EidosAssertScriptSuccess("rep(c(T, F), 2);", new EidosValue_Logical(true, false, true, false));
	EidosAssertScriptSuccess("rep(c(3, 7), 2);", new EidosValue_Int_vector(3, 7, 3, 7));
	EidosAssertScriptSuccess("rep(c(3.5, 9.1), 2);", new EidosValue_Float_vector(3.5, 9.1, 3.5, 9.1));
	EidosAssertScriptSuccess("rep(c('foo', 'bar'), 2);", new EidosValue_String("foo", "bar", "foo", "bar"));
	EidosAssertScriptSuccess("rep(c(_Test(7), _Test(2)), 2)._yolk;", new EidosValue_Int_vector(7, 2, 7, 2));
	EidosAssertScriptSuccess("rep(logical(0), 5);", new EidosValue_Logical());
	EidosAssertScriptSuccess("rep(integer(0), 5);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("rep(float(0), 5);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("rep(string(0), 5);", new EidosValue_String());
	EidosAssertScriptSuccess("rep(object(), 5);", new EidosValue_Object_vector());
	EidosAssertScriptRaise("rep(object(), c(5, 3));", 0);
	EidosAssertScriptRaise("rep(object(), integer(0));", 0);
	
	// repEach()
	EidosAssertScriptRaise("repEach(NULL, -1);", 0);
	EidosAssertScriptRaise("repEach(T, -1);", 0);
	EidosAssertScriptRaise("repEach(3, -1);", 0);
	EidosAssertScriptRaise("repEach(3.5, -1);", 0);
	EidosAssertScriptRaise("repEach('foo', -1);", 0);
	EidosAssertScriptRaise("repEach(_Test(7), -1);", 0);
	EidosAssertScriptSuccess("repEach(NULL, 0);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("repEach(T, 0);", new EidosValue_Logical());
	EidosAssertScriptSuccess("repEach(3, 0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("repEach(3.5, 0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("repEach('foo', 0);", new EidosValue_String());
	EidosAssertScriptSuccess("repEach(_Test(7), 0);", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("repEach(NULL, 2);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("repEach(T, 2);", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("repEach(3, 2);", new EidosValue_Int_vector(3, 3));
	EidosAssertScriptSuccess("repEach(3.5, 2);", new EidosValue_Float_vector(3.5, 3.5));
	EidosAssertScriptSuccess("repEach('foo', 2);", new EidosValue_String("foo", "foo"));
	EidosAssertScriptSuccess("repEach(_Test(7), 2)._yolk;", new EidosValue_Int_vector(7, 7));
	EidosAssertScriptSuccess("repEach(c(T, F), 2);", new EidosValue_Logical(true, true, false, false));
	EidosAssertScriptSuccess("repEach(c(3, 7), 2);", new EidosValue_Int_vector(3, 3, 7, 7));
	EidosAssertScriptSuccess("repEach(c(3.5, 9.1), 2);", new EidosValue_Float_vector(3.5, 3.5, 9.1, 9.1));
	EidosAssertScriptSuccess("repEach(c('foo', 'bar'), 2);", new EidosValue_String("foo", "foo", "bar", "bar"));
	EidosAssertScriptSuccess("repEach(c(_Test(7), _Test(2)), 2)._yolk;", new EidosValue_Int_vector(7, 7, 2, 2));
	EidosAssertScriptRaise("repEach(NULL, c(2,3));", 0);
	EidosAssertScriptSuccess("repEach(c(T, F), c(2,3));", new EidosValue_Logical(true, true, false, false, false));
	EidosAssertScriptSuccess("repEach(c(3, 7), c(2,3));", new EidosValue_Int_vector(3, 3, 7, 7, 7));
	EidosAssertScriptSuccess("repEach(c(3.5, 9.1), c(2,3));", new EidosValue_Float_vector(3.5, 3.5, 9.1, 9.1, 9.1));
	EidosAssertScriptSuccess("repEach(c('foo', 'bar'), c(2,3));", new EidosValue_String("foo", "foo", "bar", "bar", "bar"));
	EidosAssertScriptSuccess("repEach(c(_Test(7), _Test(2)), c(2,3))._yolk;", new EidosValue_Int_vector(7, 7, 2, 2, 2));
	EidosAssertScriptRaise("repEach(NULL, c(2,-1));", 0);
	EidosAssertScriptRaise("repEach(c(T, F), c(2,-1));", 0);
	EidosAssertScriptRaise("repEach(c(3, 7), c(2,-1));", 0);
	EidosAssertScriptRaise("repEach(c(3.5, 9.1), c(2,-1));", 0);
	EidosAssertScriptRaise("repEach(c('foo', 'bar'), c(2,-1));", 0);
	EidosAssertScriptRaise("repEach(c(_Test(7), _Test(2)), c(2,-1))._yolk;", 0);
	EidosAssertScriptRaise("repEach(NULL, c(2,3,1));", 0);
	EidosAssertScriptRaise("repEach(c(T, F), c(2,3,1));", 0);
	EidosAssertScriptRaise("repEach(c(3, 7), c(2,3,1));", 0);
	EidosAssertScriptRaise("repEach(c(3.5, 9.1), c(2,3,1));", 0);
	EidosAssertScriptRaise("repEach(c('foo', 'bar'), c(2,3,1));", 0);
	EidosAssertScriptRaise("repEach(c(_Test(7), _Test(2)), c(2,3,1))._yolk;", 0);
	EidosAssertScriptSuccess("repEach(logical(0), 5);", new EidosValue_Logical());
	EidosAssertScriptSuccess("repEach(integer(0), 5);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("repEach(float(0), 5);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("repEach(string(0), 5);", new EidosValue_String());
	EidosAssertScriptSuccess("repEach(object(), 5);", new EidosValue_Object_vector());
	EidosAssertScriptRaise("repEach(object(), c(5, 3));", 0);
	EidosAssertScriptSuccess("repEach(object(), integer(0));", new EidosValue_Object_vector());
	
	// rexp()
	EidosAssertScriptSuccess("rexp(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("setSeed(1); (rexp(3) - c(0.206919, 3.01675, 0.788416)) < 0.000001;", new EidosValue_Logical(true, true, true));
	EidosAssertScriptSuccess("setSeed(2); (rexp(3, 0.1) - c(20.7, 12.2, 0.9)) < 0.1;", new EidosValue_Logical(true, true, true));
	EidosAssertScriptSuccess("setSeed(3); (rexp(3, 0.00001) - c(95364.3, 307170.0, 74334.9)) < 0.1;", new EidosValue_Logical(true, true, true));
	EidosAssertScriptSuccess("setSeed(4); (rexp(3, c(0.1, 0.01, 0.001)) - c(2.8, 64.6, 58.8)) < 0.1;", new EidosValue_Logical(true, true, true));
	EidosAssertScriptRaise("rexp(-1);", 0);
	EidosAssertScriptRaise("rexp(3, 0.0);", 0);
	EidosAssertScriptRaise("rexp(3, 0.0);", 0);
	EidosAssertScriptRaise("rexp(3, c(0.1, 0.2));", 0);
	
	// rnorm()
	EidosAssertScriptSuccess("rnorm(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("rnorm(3, 0, 0);", new EidosValue_Float_vector(0.0, 0.0, 0.0));
	EidosAssertScriptSuccess("rnorm(3, 1, 0);", new EidosValue_Float_vector(1.0, 1.0, 1.0));
	EidosAssertScriptSuccess("setSeed(1); (rnorm(2) - c(-0.785386, 0.132009)) < 0.000001;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(2); (rnorm(2, 10.0) - c(10.38, 10.26)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(3); (rnorm(2, 10.0, 100.0) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(4); (rnorm(2, c(-10, 10), 100.0) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(5); (rnorm(2, 10.0, c(0.1, 10)) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptRaise("rnorm(-1);", 0);
	EidosAssertScriptRaise("rnorm(1, 0, -1);", 0);
	EidosAssertScriptRaise("rnorm(2, c(-10, 10, 1), 100.0);", 0);
	EidosAssertScriptRaise("rnorm(2, 10.0, c(0.1, 10, 1));", 0);
	
	// rpois()
	EidosAssertScriptSuccess("rpois(0, 1.0);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("setSeed(1); rpois(5, 1.0);", new EidosValue_Int_vector(0, 2, 0, 1, 1));
	EidosAssertScriptSuccess("setSeed(2); rpois(5, 0.2);", new EidosValue_Int_vector(1, 0, 0, 0, 0));
	EidosAssertScriptSuccess("setSeed(3); rpois(5, 10000);", new EidosValue_Int_vector(10205, 10177, 10094, 10227, 9875));
	EidosAssertScriptSuccess("setSeed(4); rpois(5, c(1, 10, 100, 1000, 10000));", new EidosValue_Int_vector(0, 8, 97, 994, 9911));
	EidosAssertScriptRaise("rpois(-1, 1.0);", 0);
	EidosAssertScriptRaise("rpois(0, 0.0);", 0);
	EidosAssertScriptRaise("setSeed(4); rpois(5, c(1, 10, 100, 1000));", 12);
	
	// runif()
	EidosAssertScriptSuccess("runif(0);", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("runif(3, 0, 0);", new EidosValue_Float_vector(0.0, 0.0, 0.0));
	EidosAssertScriptSuccess("runif(3, 1, 1);", new EidosValue_Float_vector(1.0, 1.0, 1.0));
	EidosAssertScriptSuccess("setSeed(1); (runif(2) - c(0.186915, 0.951040)) < 0.000001;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(2); (runif(2, 0.5) - c(0.93, 0.85)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(3); (runif(2, 10.0, 100.0) - c(65.31, 95.82)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(4); (runif(2, c(-100, 1), 10.0) - c(-72.52, 5.28)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("setSeed(5); (runif(2, -10.0, c(1, 1000)) - c(-8.37, 688.97)) < 0.01;", new EidosValue_Logical(true, true));
	EidosAssertScriptRaise("runif(-1);", 0);
	EidosAssertScriptRaise("runif(1, 0, -1);", 0);
	EidosAssertScriptRaise("runif(2, c(-10, 10, 1), 100.0);", 0);
	EidosAssertScriptRaise("runif(2, -10.0, c(0.1, 10, 1));", 0);
	
	// sample() – since this function treats parameter x type-agnostically, we'll test integers only (and NULL a little bit)
	EidosAssertScriptSuccess("sample(NULL, 0, T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sample(NULL, 0, F);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sample(NULL, 1, T);", 0);
	EidosAssertScriptRaise("sample(NULL, 1, F);", 0);
	EidosAssertScriptRaise("sample(1:5, -1, T);", 0);
	EidosAssertScriptRaise("sample(1:5, -1, F);", 0);
	EidosAssertScriptSuccess("sample(integer(0), 0, T);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sample(integer(0), 0, F);", new EidosValue_Int_vector());
	EidosAssertScriptRaise("sample(integer(0), 1, T);", 0);
	EidosAssertScriptRaise("sample(integer(0), 1, F);", 0);
	EidosAssertScriptSuccess("sample(5, 1, T);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("sample(5, 1, F);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("sample(5, 2, T);", new EidosValue_Int_vector(5, 5));
	EidosAssertScriptRaise("sample(5, 2, F);", 0);
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, T);", new EidosValue_Int_vector(1, 5, 3, 1, 2));
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, F);", new EidosValue_Int_vector(1, 5, 3, 2, 4));
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 6, T);", new EidosValue_Int_vector(1, 5, 3, 1, 2, 3));
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 6, F);", 12);
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, T, (1:5)^3);", new EidosValue_Int_vector(4, 5, 5, 3, 4));
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, F, (1:5)^3);", new EidosValue_Int_vector(4, 5, 3, 1, 2));
	EidosAssertScriptSuccess("setSeed(1); sample(1:5, 5, T, (0:4)^3);", new EidosValue_Int_vector(4, 5, 5, 3, 4));
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, F, (0:4)^3);", 12);
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, -1:3);", 12);
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, 1:6);", 12);
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, 1);", 12);
	
	// seq()
	EidosAssertScriptSuccess("seq(1, 5);", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("seq(5, 1);", new EidosValue_Int_vector(5, 4, 3, 2, 1));
	EidosAssertScriptSuccess("seq(1.1, 5);", new EidosValue_Float_vector(1.1, 2.1, 3.1, 4.1));
	EidosAssertScriptSuccess("seq(1, 5.1);", new EidosValue_Float_vector(1, 2, 3, 4, 5));
	EidosAssertScriptSuccess("seq(1, 10, 2);", new EidosValue_Int_vector(1, 3, 5, 7, 9));
	EidosAssertScriptRaise("seq(1, 10, -2);", 0);
	EidosAssertScriptSuccess("seq(10, 1, -2);", new EidosValue_Int_vector(10, 8, 6, 4, 2));
	EidosAssertScriptSuccess("(seq(1, 2, 0.2) - c(1, 1.2, 1.4, 1.6, 1.8, 2.0)) < 0.000000001;", new EidosValue_Logical(true, true, true, true, true, true));
	EidosAssertScriptRaise("seq(1, 2, -0.2);", 0);
	EidosAssertScriptSuccess("(seq(2, 1, -0.2) - c(2.0, 1.8, 1.6, 1.4, 1.2, 1)) < 0.000000001;", new EidosValue_Logical(true, true, true, true, true, true));
	EidosAssertScriptRaise("seq('foo', 2, 1);", 0);
	EidosAssertScriptRaise("seq(1, 'foo', 2);", 0);
	EidosAssertScriptRaise("seq(2, 1, 'foo');", 0);
	EidosAssertScriptRaise("seq(T, 2, 1);", 0);
	EidosAssertScriptRaise("seq(1, T, 2);", 0);
	EidosAssertScriptRaise("seq(2, 1, T);", 0);
	EidosAssertScriptRaise("seq(NULL, 2, 1);", 0);
	EidosAssertScriptRaise("seq(1, NULL, 2);", 0);
	EidosAssertScriptRaise("seq(2, 1, NULL);", 0);
	
	// seqAlong()
	EidosAssertScriptSuccess("seqAlong(NULL);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("seqAlong(logical(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("seqAlong(object());", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("seqAlong(5);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("seqAlong(5.1);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("seqAlong('foo');", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("seqAlong(5:9);", new EidosValue_Int_vector(0, 1, 2, 3, 4));
	EidosAssertScriptSuccess("seqAlong(5.1:9.5);", new EidosValue_Int_vector(0, 1, 2, 3, 4));
	EidosAssertScriptSuccess("seqAlong(c('foo', 'bar', 'baz'));", new EidosValue_Int_vector(0, 1, 2));
	
	// string()
	EidosAssertScriptSuccess("string(0);", new EidosValue_String());
	EidosAssertScriptSuccess("string(1);", new EidosValue_String(""));
	EidosAssertScriptSuccess("string(2);", new EidosValue_String("", ""));
	EidosAssertScriptSuccess("string(5);", new EidosValue_String("", "", "", "", ""));
	EidosAssertScriptRaise("string(-1);", 0);
	EidosAssertScriptRaise("string(-10000);", 0);
	EidosAssertScriptRaise("string(NULL);", 0);
	EidosAssertScriptRaise("string(integer(0));", 0);
	
	#pragma mark value inspection / manipulation
	
	// all()
	EidosAssertScriptRaise("all(NULL);", 0);
	EidosAssertScriptRaise("all(0);", 0);
	EidosAssertScriptRaise("all(0.5);", 0);
	EidosAssertScriptRaise("all('foo');", 0);
	EidosAssertScriptRaise("all(_Test(7));", 0);
	EidosAssertScriptSuccess("all(logical(0));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("all(T);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("all(F);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("all(c(T,T,T,T,T,T,T,T,T,T));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("all(c(T,T,T,T,T,T,T,F,T,T));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("all(c(F,F,F,F,F,F,F,F,F,F));", new EidosValue_Logical(false));
	
	// any()
	EidosAssertScriptRaise("any(NULL);", 0);
	EidosAssertScriptRaise("any(0);", 0);
	EidosAssertScriptRaise("any(0.5);", 0);
	EidosAssertScriptRaise("any('foo');", 0);
	EidosAssertScriptRaise("any(_Test(7));", 0);
	EidosAssertScriptSuccess("any(logical(0));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("any(T);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("any(F);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("any(c(T,T,T,T,T,T,T,T,T,T));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("any(c(T,T,T,T,T,T,T,F,T,T));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("any(c(F,F,F,F,F,F,F,F,F,F));", new EidosValue_Logical(false));
	
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
	
	// ifelse() – since this function treats parameters trueValues and falseValues type-agnostically, we'll test integers only (and NULL a little bit)
	EidosAssertScriptRaise("ifelse(NULL, integer(0), integer(0));", 0);
	EidosAssertScriptRaise("ifelse(logical(0), NULL, integer(0));", 0);
	EidosAssertScriptRaise("ifelse(logical(0), integer(0), NULL);", 0);
	EidosAssertScriptSuccess("ifelse(logical(0), integer(0), integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptRaise("ifelse(logical(0), 5, 2);", 0);
	EidosAssertScriptRaise("ifelse(T, integer(0), integer(0));", 0);
	EidosAssertScriptRaise("ifelse(T, 5, 2:3);", 0);
	EidosAssertScriptRaise("ifelse(T, 5:6, 2);", 0);
	EidosAssertScriptRaise("ifelse(c(T,T), 5, 2);", 0);
	EidosAssertScriptSuccess("ifelse(T, 5, 2);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("ifelse(F, 5, 2);", new EidosValue_Int_singleton_const(2));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), 1:6, -6:-1);", new EidosValue_Int_vector(1, -5, -4, 4, -2, 6));
	
	// nchar()
	EidosAssertScriptRaise("nchar(NULL);", 0);
	EidosAssertScriptRaise("nchar(T);", 0);
	EidosAssertScriptRaise("nchar(5);", 0);
	EidosAssertScriptRaise("nchar(5.5);", 0);
	EidosAssertScriptRaise("nchar(_Test(7));", 0);
	EidosAssertScriptSuccess("nchar('');", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("nchar(' ');", new EidosValue_Int_singleton_const(1));
	EidosAssertScriptSuccess("nchar('abcde');", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("nchar('abc\tde');", new EidosValue_Int_singleton_const(6));
	EidosAssertScriptSuccess("nchar(c('', 'abcde', '', 'wumpus'));", new EidosValue_Int_vector(0, 5, 0, 6));
	
	// paste()
	EidosAssertScriptSuccess("paste(NULL);", new EidosValue_String(""));
	EidosAssertScriptSuccess("paste(T);", new EidosValue_String("T"));
	EidosAssertScriptSuccess("paste(5);", new EidosValue_String("5"));
	EidosAssertScriptSuccess("paste(5.5);", new EidosValue_String("5.5"));
	EidosAssertScriptSuccess("paste('foo');", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("paste(_Test(7));", new EidosValue_String("_TestElement"));
	EidosAssertScriptSuccess("paste(NULL, '$$');", new EidosValue_String(""));
	EidosAssertScriptSuccess("paste(T, '$$');", new EidosValue_String("T"));
	EidosAssertScriptSuccess("paste(5, '$$');", new EidosValue_String("5"));
	EidosAssertScriptSuccess("paste(5.5, '$$');", new EidosValue_String("5.5"));
	EidosAssertScriptSuccess("paste('foo', '$$');", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("paste(_Test(7), '$$');", new EidosValue_String("_TestElement"));
	EidosAssertScriptSuccess("paste(c(T,T,F,T), '$$');", new EidosValue_String("T$$T$$F$$T"));
	EidosAssertScriptSuccess("paste(5:9, '$$');", new EidosValue_String("5$$6$$7$$8$$9"));
	EidosAssertScriptSuccess("paste(5.5:8.9, '$$');", new EidosValue_String("5.5$$6.5$$7.5$$8.5"));
	EidosAssertScriptSuccess("paste(c('foo', 'bar', 'baz'), '$$');", new EidosValue_String("foo$$bar$$baz"));
	EidosAssertScriptSuccess("paste(c(_Test(7), _Test(7), _Test(7)), '$$');", new EidosValue_String("_TestElement$$_TestElement$$_TestElement"));
	
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
	EidosAssertScriptSuccess("rev(6:10);", new EidosValue_Int_vector(10,9,8,7,6));
	EidosAssertScriptSuccess("rev(-(6:10));", new EidosValue_Int_vector(-10,-9,-8,-7,-6));
	EidosAssertScriptSuccess("rev(c('foo','bar','baz'));", new EidosValue_String("baz","bar","foo"));
	EidosAssertScriptSuccess("rev(-1);", new EidosValue_Int_singleton_const(-1));
	EidosAssertScriptSuccess("rev(1.0);", new EidosValue_Float_singleton_const(1));
	EidosAssertScriptSuccess("rev('foo');", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("rev(6.0:10);", new EidosValue_Float_vector(10,9,8,7,6));
	EidosAssertScriptSuccess("rev(c(T,T,T,F));", new EidosValue_Logical(false, true, true, true));
	
	// size()
	EidosAssertScriptSuccess("size(NULL);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("size(logical(0));", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("size(5);", new EidosValue_Int_singleton_const(1));
	EidosAssertScriptSuccess("size(c(5.5, 8.7));", new EidosValue_Int_singleton_const(2));
	EidosAssertScriptSuccess("size(c('foo', 'bar', 'baz'));", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("size(rep(_Test(7), 4));", new EidosValue_Int_singleton_const(4));
	
	// sort()
	EidosAssertScriptSuccess("sort(integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sort(integer(0), T);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sort(integer(0), F);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("sort(3);", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("sort(3, T);", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("sort(3, F);", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2));", new EidosValue_Int_vector(-3, 2, 5, 6, 19));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), T);", new EidosValue_Int_vector(-3, 2, 5, 6, 19));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), F);", new EidosValue_Int_vector(19, 6, 5, 2, -3));
	EidosAssertScriptSuccess("sort(c(T, F, T, T, F));", new EidosValue_Logical(false, false, true, true, true));
	EidosAssertScriptSuccess("sort(c(6.1, 19.3, -3.7, 5.2, 2.3));", new EidosValue_Float_vector(-3.7, 2.3, 5.2, 6.1, 19.3));
	EidosAssertScriptSuccess("sort(c('a', 'q', 'm', 'f', 'w'));", new EidosValue_String("a", "f", "m", "q", "w"));
	EidosAssertScriptRaise("sort(_Test(7));", 0);
	
	// sortBy()
	EidosAssertScriptRaise("sortBy(NULL);", 0);
	EidosAssertScriptRaise("sortBy(T);", 0);
	EidosAssertScriptRaise("sortBy(5);", 0);
	EidosAssertScriptRaise("sortBy(9.1);", 0);
	EidosAssertScriptRaise("sortBy('foo');", 0);
	EidosAssertScriptSuccess("sortBy(object(), 'foo');", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk')._yolk;", new EidosValue_Int_vector(-8, 2, 3, 7, 75));
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk', T)._yolk;", new EidosValue_Int_vector(-8, 2, 3, 7, 75));
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk', F)._yolk;", new EidosValue_Int_vector(75, 7, 3, 2, -8));
	EidosAssertScriptRaise("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_foo')._yolk;", 0);
	
	// str() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptSuccess("str(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(c(5.5, 8.7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(c('foo', 'bar', 'baz'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(rep(_Test(7), 4));", gStaticEidosValueNULL);
	
	// strsplit()
	EidosAssertScriptRaise("strsplit(NULL);", 0);
	EidosAssertScriptRaise("strsplit(T);", 0);
	EidosAssertScriptRaise("strsplit(5);", 0);
	EidosAssertScriptRaise("strsplit(5.6);", 0);
	EidosAssertScriptRaise("strsplit(string(0));", 0);
	EidosAssertScriptRaise("strsplit(string(0), '$$');", 0);
	EidosAssertScriptRaise("strsplit(c('foo', 'bar'));", 0);
	EidosAssertScriptRaise("strsplit(c('foo', 'bar'), '$$');", 0);
	EidosAssertScriptSuccess("strsplit('');", new EidosValue_String(""));
	EidosAssertScriptSuccess("strsplit('', '$$');", new EidosValue_String(""));
	EidosAssertScriptSuccess("strsplit(' ');", new EidosValue_String("", ""));
	EidosAssertScriptSuccess("strsplit('$$', '$$');", new EidosValue_String("", ""));
	EidosAssertScriptSuccess("strsplit('  ');", new EidosValue_String("", "", ""));
	EidosAssertScriptSuccess("strsplit('$$$$', '$$');", new EidosValue_String("", "", ""));
	EidosAssertScriptSuccess("strsplit('This is a test.');", new EidosValue_String("This", "is", "a", "test."));
	EidosAssertScriptSuccess("strsplit('This is a test.', '$$');", new EidosValue_String("This is a test."));
	EidosAssertScriptSuccess("strsplit('This is a test.', 'i');", new EidosValue_String("Th", "s ", "s a test."));
	EidosAssertScriptSuccess("strsplit('This is a test.', 's');", new EidosValue_String("Thi", " i", " a te", "t."));
	
	// substr()
	EidosAssertScriptSuccess("substr(string(0), 1);", new EidosValue_String());
	EidosAssertScriptSuccess("substr(string(0), 1, 2);", new EidosValue_String());
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1);", new EidosValue_String("oo", "ar", "oobaz"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 10000);", new EidosValue_String("oo", "ar", "oobaz"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 1);", new EidosValue_String("o", "a", "o"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 2);", new EidosValue_String("oo", "ar", "oo"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 3);", new EidosValue_String("oo", "ar", "oob"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3));", new EidosValue_String("oo", "r", "baz"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, c(1, 2, 3));", new EidosValue_String("o", "ar", "oob"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3), c(1, 2, 3));", new EidosValue_String("o", "r", "b"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3), c(2, 4, 6));", new EidosValue_String("oo", "r", "baz"));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 0);", new EidosValue_String("", "", ""));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, -100, 1);", new EidosValue_String("fo", "ba", "fo"));
	EidosAssertScriptRaise("x=c('foo','bar','foobaz'); substr(x, 1, c(2, 4));", 27);
	EidosAssertScriptRaise("x=c('foo','bar','foobaz'); substr(x, c(1, 2), 4);", 27);
	
	// unique()
	EidosAssertScriptSuccess("unique(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("unique(logical(0));", new EidosValue_Logical());
	EidosAssertScriptSuccess("unique(integer(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("unique(float(0));", new EidosValue_Float_vector());
	EidosAssertScriptSuccess("unique(string(0));", new EidosValue_String());
	EidosAssertScriptSuccess("unique(object());", new EidosValue_Object_vector());
	EidosAssertScriptSuccess("unique(T);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("unique(5);", new EidosValue_Int_singleton_const(5));
	EidosAssertScriptSuccess("unique(3.5);", new EidosValue_Float_singleton_const(3.5));
	EidosAssertScriptSuccess("unique('foo');", new EidosValue_String("foo"));
	EidosAssertScriptSuccess("unique(_Test(7))._yolk;", new EidosValue_Int_singleton_const(7));
	EidosAssertScriptSuccess("unique(c(T,T,T,T,F,T,T));", new EidosValue_Logical(true, false));
	EidosAssertScriptSuccess("unique(c(3,5,3,9,2,3,3,7,5));", new EidosValue_Int_vector(3, 5, 9, 2, 7));
	EidosAssertScriptSuccess("unique(c(3.5,1.2,9.3,-1.0,1.2,-1.0,1.2,7.6,3.5));", new EidosValue_Float_vector(3.5, 1.2, 9.3, -1, 7.6));
	EidosAssertScriptSuccess("unique(c('foo', 'bar', 'foo', 'baz', 'baz', 'bar', 'foo'));", new EidosValue_String("foo", "bar", "baz"));
	EidosAssertScriptSuccess("unique(c(_Test(7), _Test(7), _Test(2), _Test(7), _Test(2)))._yolk;", new EidosValue_Int_vector(7, 7, 2, 7, 2));
	
	// which()
	EidosAssertScriptRaise("which(NULL);", 0);
	EidosAssertScriptRaise("which(5);", 0);
	EidosAssertScriptRaise("which(5.7);", 0);
	EidosAssertScriptRaise("which('foo');", 0);
	EidosAssertScriptRaise("which(_Test(7));", 0);
	EidosAssertScriptSuccess("which(logical(0));", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("which(F);", new EidosValue_Int_vector());
	EidosAssertScriptSuccess("which(T);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("which(c(T,F,F,T,F,T,F,F,T));", new EidosValue_Int_vector(0, 3, 5, 8));
	
	// whichMax()
	EidosAssertScriptSuccess("whichMax(T);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMax(3);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMax(3.5);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMax('foo');", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMax(c(F, F, T, F, T));", new EidosValue_Int_singleton_const(2));
	EidosAssertScriptSuccess("whichMax(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton_const(2));
	EidosAssertScriptSuccess("whichMax(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Int_singleton_const(2));
	EidosAssertScriptSuccess("whichMax(c('foo', 'bar', 'baz'));", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptRaise("whichMax(_Test(7));", 0);
	EidosAssertScriptSuccess("whichMax(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(string(0));", gStaticEidosValueNULL);
	
	// whichMin()
	EidosAssertScriptSuccess("whichMin(T);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMin(3);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMin(3.5);", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMin('foo');", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMin(c(F, F, T, F, T));", new EidosValue_Int_singleton_const(0));
	EidosAssertScriptSuccess("whichMin(c(3, 7, 19, -5, 9));", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("whichMin(c(3.3, 7.7, 19.1, -5.8, 9.0));", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptSuccess("whichMin(c('foo', 'bar', 'baz'));", new EidosValue_Int_singleton_const(1));
	EidosAssertScriptRaise("whichMin(_Test(7));", 0);
	EidosAssertScriptSuccess("whichMin(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(string(0));", gStaticEidosValueNULL);
	
	#pragma mark value type testing / coercion
	
	// asFloat()
	EidosAssertScriptSuccess("asFloat(-1:3);", new EidosValue_Float_vector(-1,0,1,2,3));
	EidosAssertScriptSuccess("asFloat(-1.0:3);", new EidosValue_Float_vector(-1,0,1,2,3));
	EidosAssertScriptSuccess("asFloat(c(T,F,T,F));", new EidosValue_Float_vector(1,0,1,0));
	EidosAssertScriptSuccess("asFloat(c('1','2','3'));", new EidosValue_Float_vector(1,2,3));
	EidosAssertScriptRaise("asFloat('foo');", 0);
	
	// asInteger()
	EidosAssertScriptSuccess("asInteger(-1:3);", new EidosValue_Int_vector(-1,0,1,2,3));
	EidosAssertScriptSuccess("asInteger(-1.0:3);", new EidosValue_Int_vector(-1,0,1,2,3));
	EidosAssertScriptSuccess("asInteger(c(T,F,T,F));", new EidosValue_Int_vector(1,0,1,0));
	EidosAssertScriptSuccess("asInteger(c('1','2','3'));", new EidosValue_Int_vector(1,2,3));
	EidosAssertScriptRaise("asInteger('foo');", 0);
	
	// asLogical()
	EidosAssertScriptSuccess("asLogical(-1:3);", new EidosValue_Logical(true,false,true,true,true));
	EidosAssertScriptSuccess("asLogical(-1.0:3);", new EidosValue_Logical(true,false,true,true,true));
	EidosAssertScriptSuccess("asLogical(c(T,F,T,F));", new EidosValue_Logical(true,false,true,false));
	EidosAssertScriptSuccess("asLogical(c('foo','bar',''));", new EidosValue_Logical(true,true,false));
	
	// asString()
	EidosAssertScriptSuccess("asString(-1:3);", new EidosValue_String("-1","0","1","2","3"));
	EidosAssertScriptSuccess("asString(-1.0:3);", new EidosValue_String("-1","0","1","2","3"));
	EidosAssertScriptSuccess("asString(c(T,F,T,F));", new EidosValue_String("T","F","T","F"));
	EidosAssertScriptSuccess("asString(c('1','2','3'));", new EidosValue_String("1","2","3"));
	
	// elementType()
	EidosAssertScriptSuccess("elementType(NULL);", new EidosValue_String("NULL"));
	EidosAssertScriptSuccess("elementType(T);", new EidosValue_String("logical"));
	EidosAssertScriptSuccess("elementType(3);", new EidosValue_String("integer"));
	EidosAssertScriptSuccess("elementType(3.5);", new EidosValue_String("float"));
	EidosAssertScriptSuccess("elementType('foo');", new EidosValue_String("string"));
	EidosAssertScriptSuccess("elementType(_Test(7));", new EidosValue_String("_TestElement"));
	EidosAssertScriptSuccess("elementType(object());", new EidosValue_String("undefined"));
	
	// isFloat()
	EidosAssertScriptSuccess("isFloat(NULL);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isFloat(T);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isFloat(3);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isFloat(3.5);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isFloat('foo');", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isFloat(_Test(7));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isFloat(object());", new EidosValue_Logical(false));
	
	// isInteger()
	EidosAssertScriptSuccess("isInteger(NULL);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInteger(T);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInteger(3);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isInteger(3.5);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInteger('foo');", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInteger(_Test(7));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isInteger(object());", new EidosValue_Logical(false));
	
	// isLogical()
	EidosAssertScriptSuccess("isLogical(NULL);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isLogical(T);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isLogical(3);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isLogical(3.5);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isLogical('foo');", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isLogical(_Test(7));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isLogical(object());", new EidosValue_Logical(false));
	
	// isNULL()
	EidosAssertScriptSuccess("isNULL(NULL);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isNULL(T);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNULL(3);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNULL(3.5);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNULL('foo');", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNULL(_Test(7));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isNULL(object());", new EidosValue_Logical(false));
	
	// isObject()
	EidosAssertScriptSuccess("isObject(NULL);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isObject(T);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isObject(3);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isObject(3.5);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isObject('foo');", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isObject(_Test(7));", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isObject(object());", new EidosValue_Logical(true));
	
	// isString()
	EidosAssertScriptSuccess("isString(NULL);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isString(T);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isString(3);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isString(3.5);", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isString('foo');", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("isString(_Test(7));", new EidosValue_Logical(false));
	EidosAssertScriptSuccess("isString(object());", new EidosValue_Logical(false));
	
	// type()
	EidosAssertScriptSuccess("type(NULL);", new EidosValue_String("NULL"));
	EidosAssertScriptSuccess("type(T);", new EidosValue_String("logical"));
	EidosAssertScriptSuccess("type(3);", new EidosValue_String("integer"));
	EidosAssertScriptSuccess("type(3.5);", new EidosValue_String("float"));
	EidosAssertScriptSuccess("type('foo');", new EidosValue_String("string"));
	EidosAssertScriptSuccess("type(_Test(7));", new EidosValue_String("object"));
	EidosAssertScriptSuccess("type(object());", new EidosValue_String("object"));
	
	#pragma mark filesystem access
	
	// filesAtPath() – hard to know how to test this!  These tests should be true on Un*x machines, anyway – but might be disallowed by file permissions.
	EidosAssertScriptSuccess("type(filesAtPath('/tmp')) == 'string';", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("sum(filesAtPath('/') == 'bin');", new EidosValue_Int_singleton_const(1));
	EidosAssertScriptSuccess("sum(filesAtPath('/', T) == '/bin');", new EidosValue_Int_singleton_const(1));
	EidosAssertScriptSuccess("filesAtPath('foo_is_a_bad_path');", gStaticEidosValueNULL);
	
	// writeFile()
	EidosAssertScriptSuccess("writeFile('/tmp/EidosTest.txt', c(paste(0:4), paste(5:9)));", new EidosValue_Logical(true));
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("readFile('/tmp/EidosTest.txt') == c(paste(0:4), paste(5:9));", new EidosValue_Logical(true, true));
	EidosAssertScriptSuccess("all(asInteger(strsplit(paste(readFile('/tmp/EidosTest.txt')))) == 0:9);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("readFile('foo_is_a_bad_path.txt');", gStaticEidosValueNULL);
	
	#pragma mark miscellaneous
	
	// date()
	EidosAssertScriptSuccess("size(strsplit(date(), '-'));", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptRaise("date(NULL);", 0);
	EidosAssertScriptRaise("date(T);", 0);
	EidosAssertScriptRaise("date(3);", 0);
	EidosAssertScriptRaise("date(3.5);", 0);
	EidosAssertScriptRaise("date('foo');", 0);
	EidosAssertScriptRaise("date(_Test(7));", 0);
	
	// executeLambda()
	EidosAssertScriptSuccess("x=7; executeLambda('x^2;');", new EidosValue_Float_singleton_const(49));
	EidosAssertScriptRaise("x=7; executeLambda('x^2');", 5);
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'));", 5);
	EidosAssertScriptRaise("x=7; executeLambda(string(0));", 5);
	EidosAssertScriptSuccess("x=7; executeLambda('x=x^2+4;'); x;", new EidosValue_Float_singleton_const(53));
	EidosAssertScriptRaise("executeLambda(NULL);", 0);
	EidosAssertScriptRaise("executeLambda(T);", 0);
	EidosAssertScriptRaise("executeLambda(3);", 0);
	EidosAssertScriptRaise("executeLambda(3.5);", 0);
	EidosAssertScriptRaise("executeLambda(_Test(7));", 0);
	
	// function()
	EidosAssertScriptSuccess("function();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function('function');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function('foo');", gStaticEidosValueNULL);	// does not throw at present
	EidosAssertScriptRaise("function(string(0));", 0);
	EidosAssertScriptRaise("function(NULL);", 0);
	EidosAssertScriptRaise("function(T);", 0);
	EidosAssertScriptRaise("function(3);", 0);
	EidosAssertScriptRaise("function(3.5);", 0);
	EidosAssertScriptRaise("function(_Test(7));", 0);
	
	// globals()
	EidosAssertScriptSuccess("globals();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("globals(NULL);", 0);
	EidosAssertScriptRaise("globals(T);", 0);
	EidosAssertScriptRaise("globals(3);", 0);
	EidosAssertScriptRaise("globals(3.5);", 0);
	EidosAssertScriptRaise("globals('foo');", 0);
	EidosAssertScriptRaise("globals(_Test(7));", 0);
	
	// help()
	EidosAssertScriptSuccess("help();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("help('help');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("help('foo');", gStaticEidosValueNULL);	// does not throw at present
	EidosAssertScriptRaise("help(string(0));", 0);
	EidosAssertScriptRaise("help(NULL);", 0);
	EidosAssertScriptRaise("help(T);", 0);
	EidosAssertScriptRaise("help(3);", 0);
	EidosAssertScriptRaise("help(3.5);", 0);
	EidosAssertScriptRaise("help(_Test(7));", 0);
	
	// license()
	EidosAssertScriptSuccess("license();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("license(NULL);", 0);
	EidosAssertScriptRaise("license(T);", 0);
	EidosAssertScriptRaise("license(3);", 0);
	EidosAssertScriptRaise("license(3.5);", 0);
	EidosAssertScriptRaise("license('foo');", 0);
	EidosAssertScriptRaise("license(_Test(7));", 0);
	
	// rm()
	EidosAssertScriptRaise("x=37; rm('x'); x;", 15);
	EidosAssertScriptSuccess("x=37; rm('y'); x;", new EidosValue_Int_singleton_const(37));
	EidosAssertScriptRaise("x=37; rm(); x;", 12);
	EidosAssertScriptRaise("rm(3);", 0);
	EidosAssertScriptRaise("rm(3.5);", 0);
	EidosAssertScriptRaise("rm(_Test(7));", 0);
	EidosAssertScriptRaise("rm(T);", 0);
	EidosAssertScriptRaise("rm(F);", 0);
	EidosAssertScriptRaise("rm(NULL);", 0);
	EidosAssertScriptRaise("rm(INF);", 0);
	EidosAssertScriptRaise("rm(NAN);", 0);
	EidosAssertScriptRaise("rm(E);", 0);
	EidosAssertScriptRaise("rm(PI);", 0);

	// setSeed()
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(5); y=runif(10); all(x==y);", new EidosValue_Logical(true));
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(6); y=runif(10); all(x==y);", new EidosValue_Logical(false));
	EidosAssertScriptRaise("setSeed(NULL);", 0);
	EidosAssertScriptRaise("setSeed(T);", 0);
	EidosAssertScriptRaise("setSeed(3.5);", 0);
	EidosAssertScriptRaise("setSeed('foo');", 0);
	EidosAssertScriptRaise("setSeed(_Test(7));", 0);
	
	// getSeed()
	EidosAssertScriptSuccess("setSeed(13); getSeed();", new EidosValue_Int_singleton_const(13));
	EidosAssertScriptSuccess("setSeed(13); setSeed(7); getSeed();", new EidosValue_Int_singleton_const(7));
	EidosAssertScriptRaise("getSeed(NULL);", 0);
	EidosAssertScriptRaise("getSeed(T);", 0);
	EidosAssertScriptRaise("getSeed(3);", 0);
	EidosAssertScriptRaise("getSeed(3.5);", 0);
	EidosAssertScriptRaise("getSeed('foo');", 0);
	EidosAssertScriptRaise("getSeed(_Test(7));", 0);
	
	// stop()
	EidosAssertScriptRaise("stop();", 0);
	EidosAssertScriptRaise("stop('Error');", 0);
	EidosAssertScriptRaise("stop(NULL);", 0);
	EidosAssertScriptRaise("stop(T);", 0);
	EidosAssertScriptRaise("stop(3);", 0);
	EidosAssertScriptRaise("stop(3.5);", 0);
	EidosAssertScriptRaise("stop(_Test(7));", 0);
	
	// time()
	EidosAssertScriptSuccess("size(strsplit(time(), ':'));", new EidosValue_Int_singleton_const(3));
	EidosAssertScriptRaise("time(NULL);", 0);
	EidosAssertScriptRaise("time(T);", 0);
	EidosAssertScriptRaise("time(3);", 0);
	EidosAssertScriptRaise("time(3.5);", 0);
	EidosAssertScriptRaise("time('foo');", 0);
	EidosAssertScriptRaise("time(_Test(7));", 0);
	
	// version()
	EidosAssertScriptSuccess("version();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("version(NULL);", 0);
	EidosAssertScriptRaise("version(T);", 0);
	EidosAssertScriptRaise("version(3);", 0);
	EidosAssertScriptRaise("version(3.5);", 0);
	EidosAssertScriptRaise("version('foo');", 0);
	EidosAssertScriptRaise("version(_Test(7));", 0);
	
#pragma mark code examples
	
	// Fibonacci sequence; see Eidos manual section 2.6.1-ish
	std::vector<int> correct_fibs = {1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987,1597,2584,4181,6765};
	
	EidosAssertScriptSuccess(	"fib = c(1, 1);												\
								while (size(fib) < 20)										\
								{															\
									next_fib = fib[size(fib) - 1] + fib[size(fib) - 2];		\
									fib = c(fib, next_fib);									\
								}															\
								fib;",
							 new EidosValue_Int_vector(correct_fibs));
	
	EidosAssertScriptSuccess(	"counter = 12;							\
								factorial = 1;							\
								do										\
								{										\
									factorial = factorial * counter;	\
									counter = counter - 1;				\
								}										\
								while (counter > 0);					\
								factorial;",
							 new EidosValue_Int_singleton_const(479001600));
	
	std::vector<int> correct_primes = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199};
	
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
							 new EidosValue_Int_vector(correct_primes));
	
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































































