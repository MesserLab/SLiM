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
void AssertScriptSuccess(const string &p_script_string, EidosValue *p_correct_result);
void AssertScriptRaise(const string &p_script_string);

// Keeping records of test success / failure
static int gTestSuccessCount = 0;
static int gTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void AssertScriptSuccess(const string &p_script_string, EidosValue *p_correct_result)
{
	EidosScript script(p_script_string, 0);
	EidosValue *result = nullptr;
	EidosSymbolTable symbol_table;
	
	gTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
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
		if (CompareEidosValues(result, value_index, p_correct_result, value_index) != 0)
		{
			std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : mismatched values (" << *result << "), expected (" << *p_correct_result << ")" << endl;
			return;
		}
	}
	
	gTestFailureCount--;	// correct for our assumption of failure above
	gTestSuccessCount++;
	
	//std::cerr << p_script_string << " == " << p_correct_result->Type() << "(" << *p_correct_result << ") : \e[32mSUCCESS\e[0m" << endl;
}

// Instantiates and runs the script, and prints an error if the script does not cause an exception to be raised
void AssertScriptRaise(const string &p_script_string)
{
	EidosScript script(p_script_string, 0);
	EidosSymbolTable symbol_table;
	
	try {
		script.Tokenize();
		script.ParseInterpreterBlockToAST();
		
		EidosInterpreter interpreter(script, symbol_table);
		
		// note InjectIntoInterpreter() is not called here; we want a pristine environment to test the language itself
		
		interpreter.EvaluateInterpreterBlock(true);
		
		gTestFailureCount++;
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : no raise during EvaluateInterpreterBlock()." << endl;
	}
	catch (std::runtime_error err)
	{
		gTestSuccessCount++;
		
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
		//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : \e[32mSUCCESS\e[0m" << endl;
		return;
	}
}

void RunEidosTests(void)
{
	// Reset error counts
	gTestSuccessCount = 0;
	gTestFailureCount = 0;
	
	// test literals, built-in identifiers, and tokenization
	AssertScriptSuccess("3;", new EidosValue_Int_singleton_const(3));
	AssertScriptSuccess("3e2;", new EidosValue_Int_singleton_const(300));
	AssertScriptSuccess("3.1;", new EidosValue_Float_singleton_const(3.1));
	AssertScriptSuccess("3.1e2;", new EidosValue_Float_singleton_const(3.1e2));
	AssertScriptSuccess("3.1e-2;", new EidosValue_Float_singleton_const(3.1e-2));
	AssertScriptSuccess("\"foo\";", new EidosValue_String("foo"));
	AssertScriptSuccess("\"foo\\tbar\";", new EidosValue_String("foo\tbar"));
	AssertScriptSuccess("T;", new EidosValue_Logical(true));
	AssertScriptSuccess("F;", new EidosValue_Logical(false));
	AssertScriptRaise("$foo;");
	
	// ************************************************************************************
	//
	//	Operator tests
	//
	
	// test vector-to-singleton comparisons for integers
	AssertScriptSuccess("rep(1:3, 2) == 2;", new EidosValue_Logical(false, true, false, false, true, false));
	AssertScriptSuccess("rep(1:3, 2) != 2;", new EidosValue_Logical(true, false, true, true, false, true));
	AssertScriptSuccess("rep(1:3, 2) < 2;", new EidosValue_Logical(true, false, false, true, false, false));
	AssertScriptSuccess("rep(1:3, 2) <= 2;", new EidosValue_Logical(true, true, false, true, true, false));
	AssertScriptSuccess("rep(1:3, 2) > 2;", new EidosValue_Logical(false, false, true, false, false, true));
	AssertScriptSuccess("rep(1:3, 2) >= 2;", new EidosValue_Logical(false, true, true, false, true, true));
	
	AssertScriptSuccess("2 == rep(1:3, 2);", new EidosValue_Logical(false, true, false, false, true, false));
	AssertScriptSuccess("2 != rep(1:3, 2);", new EidosValue_Logical(true, false, true, true, false, true));
	AssertScriptSuccess("2 > rep(1:3, 2);", new EidosValue_Logical(true, false, false, true, false, false));
	AssertScriptSuccess("2 >= rep(1:3, 2);", new EidosValue_Logical(true, true, false, true, true, false));
	AssertScriptSuccess("2 < rep(1:3, 2);", new EidosValue_Logical(false, false, true, false, false, true));
	AssertScriptSuccess("2 <= rep(1:3, 2);", new EidosValue_Logical(false, true, true, false, true, true));
	
	// operator +
	AssertScriptSuccess("1+1;", new EidosValue_Int_singleton_const(2));
	AssertScriptSuccess("1+-1;", new EidosValue_Int_singleton_const(0));
	AssertScriptSuccess("(0:2)+10;", new EidosValue_Int_vector(10, 11, 12));
	AssertScriptSuccess("10+(0:2);", new EidosValue_Int_vector(10, 11, 12));
	AssertScriptSuccess("(15:13)+(0:2);", new EidosValue_Int_vector(15, 15, 15));
	AssertScriptRaise("(15:12)+(0:2);");
	AssertScriptRaise("NULL+(0:2);");		// FIXME should this be an error?
	AssertScriptSuccess("1+1.0;", new EidosValue_Float_singleton_const(2));
	AssertScriptSuccess("1.0+1;", new EidosValue_Float_singleton_const(2));
	AssertScriptSuccess("1.0+-1.0;", new EidosValue_Float_singleton_const(0));
	AssertScriptSuccess("(0:2.0)+10;", new EidosValue_Float_vector(10, 11, 12));
	AssertScriptSuccess("10.0+(0:2);", new EidosValue_Float_vector(10, 11, 12));
	AssertScriptSuccess("(15.0:13)+(0:2.0);", new EidosValue_Float_vector(15, 15, 15));
	AssertScriptRaise("(15:12.0)+(0:2);");
	AssertScriptRaise("NULL+(0:2.0);");		// FIXME should this be an error?
	AssertScriptSuccess("\"foo\"+5;", new EidosValue_String("foo5"));
	AssertScriptSuccess("\"foo\"+5.0;", new EidosValue_String("foo5"));
	AssertScriptSuccess("\"foo\"+5.1;", new EidosValue_String("foo5.1"));
	AssertScriptSuccess("5+\"foo\";", new EidosValue_String("5foo"));
	AssertScriptSuccess("5.0+\"foo\";", new EidosValue_String("5foo"));
	AssertScriptSuccess("5.1+\"foo\";", new EidosValue_String("5.1foo"));
	AssertScriptSuccess("\"foo\"+1:3;", new EidosValue_String("foo1", "foo2", "foo3"));
	AssertScriptSuccess("1:3+\"foo\";", new EidosValue_String("1foo", "2foo", "3foo"));
	AssertScriptSuccess("NULL+\"foo\";", new EidosValue_String());		// FIXME should this be an error?
	AssertScriptSuccess("\"foo\"+\"bar\";", new EidosValue_String("foobar"));
	AssertScriptSuccess("\"foo\"+c(\"bar\", \"baz\");", new EidosValue_String("foobar", "foobaz"));
	AssertScriptSuccess("c(\"bar\", \"baz\")+\"foo\";", new EidosValue_String("barfoo", "bazfoo"));
	AssertScriptSuccess("c(\"bar\", \"baz\")+T;", new EidosValue_String("barT", "bazT"));
	AssertScriptSuccess("F+c(\"bar\", \"baz\");", new EidosValue_String("Fbar", "Fbaz"));
	AssertScriptRaise("T+F;");
	AssertScriptRaise("T+T;");
	AssertScriptRaise("F+F;");
	AssertScriptSuccess("+5;", new EidosValue_Int_singleton_const(5));
	AssertScriptSuccess("+5.0;", new EidosValue_Float_singleton_const(5));
	AssertScriptRaise("+\"foo\";");
	AssertScriptRaise("+T;");
	AssertScriptSuccess("3+4+5;", new EidosValue_Int_singleton_const(12));
	
	// operator -
	AssertScriptSuccess("1-1;", new EidosValue_Int_singleton_const(0));
	AssertScriptSuccess("1--1;", new EidosValue_Int_singleton_const(2));
	AssertScriptSuccess("(0:2)-10;", new EidosValue_Int_vector(-10, -9, -8));
	AssertScriptSuccess("10-(0:2);", new EidosValue_Int_vector(10, 9, 8));
	AssertScriptSuccess("(15:13)-(0:2);", new EidosValue_Int_vector(15, 13, 11));
	AssertScriptRaise("(15:12)-(0:2);");
	AssertScriptRaise("NULL-(0:2);");		// FIXME should this be an error?
	AssertScriptSuccess("1-1.0;", new EidosValue_Float_singleton_const(0));
	AssertScriptSuccess("1.0-1;", new EidosValue_Float_singleton_const(0));
	AssertScriptSuccess("1.0--1.0;", new EidosValue_Float_singleton_const(2));
	AssertScriptSuccess("(0:2.0)-10;", new EidosValue_Float_vector(-10, -9, -8));
	AssertScriptSuccess("10.0-(0:2);", new EidosValue_Float_vector(10, 9, 8));
	AssertScriptSuccess("(15.0:13)-(0:2.0);", new EidosValue_Float_vector(15, 13, 11));
	AssertScriptRaise("(15:12.0)-(0:2);");
	AssertScriptRaise("NULL-(0:2.0);");		// FIXME should this be an error?
	AssertScriptRaise("\"foo\"-1;");
	AssertScriptRaise("T-F;");
	AssertScriptRaise("T-T;");
	AssertScriptRaise("F-F;");
	AssertScriptSuccess("-5;", new EidosValue_Int_singleton_const(-5));
	AssertScriptSuccess("-5.0;", new EidosValue_Float_singleton_const(-5));
	AssertScriptRaise("-\"foo\";");
	AssertScriptRaise("-T;");
	AssertScriptSuccess("3-4-5;", new EidosValue_Int_singleton_const(-6));
	
    // operator *
    AssertScriptSuccess("1*1;", new EidosValue_Int_singleton_const(1));
    AssertScriptSuccess("1*-1;", new EidosValue_Int_singleton_const(-1));
    AssertScriptSuccess("(0:2)*10;", new EidosValue_Int_vector(0, 10, 20));
    AssertScriptSuccess("10*(0:2);", new EidosValue_Int_vector(0, 10, 20));
    AssertScriptSuccess("(15:13)*(0:2);", new EidosValue_Int_vector(0, 14, 26));
    AssertScriptRaise("(15:12)*(0:2);");
    AssertScriptRaise("NULL*(0:2);");		// FIXME should this be an error?
    AssertScriptSuccess("1*1.0;", new EidosValue_Float_singleton_const(1));
    AssertScriptSuccess("1.0*1;", new EidosValue_Float_singleton_const(1));
    AssertScriptSuccess("1.0*-1.0;", new EidosValue_Float_singleton_const(-1));
    AssertScriptSuccess("(0:2.0)*10;", new EidosValue_Float_vector(0, 10, 20));
    AssertScriptSuccess("10.0*(0:2);", new EidosValue_Float_vector(0, 10, 20));
    AssertScriptSuccess("(15.0:13)*(0:2.0);", new EidosValue_Float_vector(0, 14, 26));
    AssertScriptRaise("(15:12.0)*(0:2);");
    AssertScriptRaise("NULL*(0:2.0);");		// FIXME should this be an error?
    AssertScriptRaise("\"foo\"*5;");
    AssertScriptRaise("T*F;");
    AssertScriptRaise("T*T;");
    AssertScriptRaise("F*F;");
    AssertScriptRaise("*5;");
    AssertScriptRaise("*5.0;");
    AssertScriptRaise("*\"foo\";");
    AssertScriptRaise("*T;");
    AssertScriptSuccess("3*4*5;", new EidosValue_Int_singleton_const(60));
    
    // operator /
    AssertScriptSuccess("1/1;", new EidosValue_Float_singleton_const(1));
    AssertScriptSuccess("1/-1;", new EidosValue_Float_singleton_const(-1));
    AssertScriptSuccess("(0:2)/10;", new EidosValue_Float_vector(0, 0.1, 0.2));
    AssertScriptRaise("(15:12)/(0:2);");
    AssertScriptRaise("NULL/(0:2);");		// FIXME should this be an error?
    AssertScriptSuccess("1/1.0;", new EidosValue_Float_singleton_const(1));
    AssertScriptSuccess("1.0/1;", new EidosValue_Float_singleton_const(1));
    AssertScriptSuccess("1.0/-1.0;", new EidosValue_Float_singleton_const(-1));
    AssertScriptSuccess("(0:2.0)/10;", new EidosValue_Float_vector(0, 0.1, 0.2));
    AssertScriptSuccess("10.0/(0:2);", new EidosValue_Float_vector(std::numeric_limits<double>::infinity(), 10, 5));
    AssertScriptSuccess("(15.0:13)/(0:2.0);", new EidosValue_Float_vector(std::numeric_limits<double>::infinity(), 14, 6.5));
    AssertScriptRaise("(15:12.0)/(0:2);");
    AssertScriptRaise("NULL/(0:2.0);");		// FIXME should this be an error?
    AssertScriptRaise("\"foo\"/5;");
    AssertScriptRaise("T/F;");
    AssertScriptRaise("T/T;");
    AssertScriptRaise("F/F;");
    AssertScriptRaise("/5;");
    AssertScriptRaise("/5.0;");
    AssertScriptRaise("/\"foo\";");
    AssertScriptRaise("/T;");
    AssertScriptSuccess("3/4/5;", new EidosValue_Float_singleton_const(0.15));
	AssertScriptSuccess("6/0;", new EidosValue_Float_singleton_const(std::numeric_limits<double>::infinity()));
    
    // operator %
    AssertScriptSuccess("1%1;", new EidosValue_Float_singleton_const(0));
    AssertScriptSuccess("1%-1;", new EidosValue_Float_singleton_const(0));
    AssertScriptSuccess("(0:2)%10;", new EidosValue_Float_vector(0, 1, 2));
    AssertScriptRaise("(15:12)%(0:2);");
    AssertScriptRaise("NULL%(0:2);");       // FIXME should this be an error?
    AssertScriptSuccess("1%1.0;", new EidosValue_Float_singleton_const(0));
    AssertScriptSuccess("1.0%1;", new EidosValue_Float_singleton_const(0));
    AssertScriptSuccess("1.0%-1.0;", new EidosValue_Float_singleton_const(0));
    AssertScriptSuccess("(0:2.0)%10;", new EidosValue_Float_vector(0, 1, 2));
    AssertScriptSuccess("10.0%(0:4);", new EidosValue_Float_vector(std::numeric_limits<double>::quiet_NaN(), 0, 0, 1, 2));
    AssertScriptSuccess("(15.0:13)%(0:2.0);", new EidosValue_Float_vector(std::numeric_limits<double>::quiet_NaN(), 0, 1));
    AssertScriptRaise("(15:12.0)%(0:2);");
    AssertScriptRaise("NULL%(0:2.0);");		// FIXME should this be an error?
    AssertScriptRaise("\"foo\"%5;");
    AssertScriptRaise("T%F;");
    AssertScriptRaise("T%T;");
    AssertScriptRaise("F%F;");
    AssertScriptRaise("%5;");
    AssertScriptRaise("%5.0;");
    AssertScriptRaise("%\"foo\";");
    AssertScriptRaise("%T;");
    AssertScriptSuccess("3%4%5;", new EidosValue_Float_singleton_const(3));

	// operator = (especially in conjunction with operator [])
	AssertScriptSuccess("x = 5; x;", new EidosValue_Int_singleton_const(5));
	AssertScriptSuccess("x = 1:5; x;", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	AssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 10; x;", new EidosValue_Int_vector(10, 2, 10, 4, 10));
	AssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 10; x;", new EidosValue_Int_vector(1, 2, 10, 4, 10));
	AssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 10; x;", new EidosValue_Int_vector(10, 2, 10, 4, 10));
	AssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 10; x;", new EidosValue_Int_vector(10, 2, 10, 4, 5));
	AssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 11:13; x;", new EidosValue_Int_vector(11, 2, 12, 4, 13));
	AssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 11:12; x;", new EidosValue_Int_vector(1, 2, 11, 4, 12));
	AssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 11:13; x;", new EidosValue_Int_vector(11, 2, 12, 4, 13));
	AssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 11:12; x;", new EidosValue_Int_vector(11, 2, 12, 4, 5));
	AssertScriptRaise("x = 1:5; x[1:3*2 - 2][0:1] = 11:13; x;");
	AssertScriptRaise("x = 1:5; x[NULL] = NULL; x;");
	AssertScriptSuccess("x = 1:5; x[NULL] = 10; x;", new EidosValue_Int_vector(1, 2, 3, 4, 5)); // assigns 10 to no indices, perfectly legal
	AssertScriptRaise("x = 1:5; x[3] = NULL; x;");
	AssertScriptSuccess("x = 1.0:5; x[3] = 1; x;", new EidosValue_Float_vector(1, 2, 3, 1, 5));
	AssertScriptSuccess("x = c(\"a\", \"b\", \"c\"); x[1] = 1; x;", new EidosValue_String("a", "1", "c"));
	AssertScriptRaise("x = 1:5; x[3] = 1.5; x;");
	AssertScriptRaise("x = 1:5; x[3] = \"foo\"; x;");
	AssertScriptSuccess("x = 5; x[0] = 10; x;", new EidosValue_Int_singleton_const(10));
	AssertScriptSuccess("x = 5.0; x[0] = 10.0; x;", new EidosValue_Float_singleton_const(10));
	AssertScriptRaise("x = 5; x[0] = 10.0; x;");
	AssertScriptSuccess("x = 5.0; x[0] = 10; x;", new EidosValue_Float_singleton_const(10));
	AssertScriptSuccess("x = T; x[0] = F; x;", new EidosValue_Logical(false));
	AssertScriptSuccess("x = \"foo\"; x[0] = \"bar\"; x;", new EidosValue_String("bar"));
	
	// operator = (especially in conjunction with operator .)
	AssertScriptSuccess("x=_Test(9); x._yolk;", new EidosValue_Int_singleton_const(9));
	AssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk;", new EidosValue_Int_vector(9, 7, 9, 7));
	AssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=2; z._yolk;", new EidosValue_Int_vector(9, 2, 9, 2));
	AssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=2; z._yolk;", new EidosValue_Int_vector(9, 2, 9, 2));
	AssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[c(1,0)]._yolk=c(2, 5); z._yolk;", new EidosValue_Int_vector(5, 2, 5, 2));
	AssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[c(1,0)]=c(3, 6); z._yolk;", new EidosValue_Int_vector(6, 3, 6, 3));
	AssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=6.5; z._yolk;");
	AssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=6.5; z._yolk;");
	AssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2:3]._yolk=6.5; z._yolk;");
	AssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[2:3]=6.5; z._yolk;");
	AssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2]=6.5; z._yolk;");
	
	// operator >
	AssertScriptSuccess("T > F;", new EidosValue_Logical(true));
	AssertScriptSuccess("F > T;", new EidosValue_Logical(false));
	AssertScriptSuccess("T > -5;", new EidosValue_Logical(true));
	AssertScriptSuccess("-5 > T;", new EidosValue_Logical(false));
	AssertScriptSuccess("T > 5;", new EidosValue_Logical(false));
	AssertScriptSuccess("5 > T;", new EidosValue_Logical(true));
	AssertScriptSuccess("T > -5.0;", new EidosValue_Logical(true));
	AssertScriptSuccess("-5.0 > T;", new EidosValue_Logical(false));
	AssertScriptSuccess("T > 5.0;", new EidosValue_Logical(false));
	AssertScriptSuccess("5.0 > T;", new EidosValue_Logical(true));
	AssertScriptSuccess("T > \"FOO\";", new EidosValue_Logical(true));
	AssertScriptSuccess("\"FOO\" > T;", new EidosValue_Logical(false));
	AssertScriptSuccess("T > \"XYZZY\";", new EidosValue_Logical(false));
	AssertScriptSuccess("\"XYZZY\" > T;", new EidosValue_Logical(true));
	AssertScriptSuccess("5 > -10;", new EidosValue_Logical(true));
	AssertScriptSuccess("-10 > 5;", new EidosValue_Logical(false));
	AssertScriptSuccess("5.0 > -10;", new EidosValue_Logical(true));
	AssertScriptSuccess("-10 > 5.0;", new EidosValue_Logical(false));
	AssertScriptSuccess("5 > -10.0;", new EidosValue_Logical(true));
	AssertScriptSuccess("-10.0 > 5;", new EidosValue_Logical(false));
	AssertScriptSuccess("\"foo\" > \"bar\";", new EidosValue_Logical(true));
	AssertScriptSuccess("\"bar\" > \"foo\";", new EidosValue_Logical(false));
	AssertScriptSuccess("120 > \"10\";", new EidosValue_Logical(true));
	AssertScriptSuccess("10 > \"120\";", new EidosValue_Logical(false));
	AssertScriptSuccess("120 > \"15\";", new EidosValue_Logical(false));
	AssertScriptSuccess("15 > \"120\";", new EidosValue_Logical(true));
	AssertScriptRaise("_Test(9) > 5");
	AssertScriptRaise("5 > _Test(9)");
	AssertScriptSuccess("NULL > 5;", new EidosValue_Logical());
	AssertScriptSuccess("NULL > 5.0;", new EidosValue_Logical());
	AssertScriptSuccess("NULL > \"foo\";", new EidosValue_Logical());
	AssertScriptSuccess("5 > NULL;", new EidosValue_Logical());
	AssertScriptSuccess("5.0 > NULL;", new EidosValue_Logical());
	AssertScriptSuccess("\"foo\" > NULL;", new EidosValue_Logical());
	
	// operator <
	AssertScriptSuccess("T < F;", new EidosValue_Logical(false));
	AssertScriptSuccess("F < T;", new EidosValue_Logical(true));
	AssertScriptSuccess("T < -5;", new EidosValue_Logical(false));
	AssertScriptSuccess("-5 < T;", new EidosValue_Logical(true));
	AssertScriptSuccess("T < 5;", new EidosValue_Logical(true));
	AssertScriptSuccess("5 < T;", new EidosValue_Logical(false));
	AssertScriptSuccess("T < -5.0;", new EidosValue_Logical(false));
	AssertScriptSuccess("-5.0 < T;", new EidosValue_Logical(true));
	AssertScriptSuccess("T < 5.0;", new EidosValue_Logical(true));
	AssertScriptSuccess("5.0 < T;", new EidosValue_Logical(false));
	AssertScriptSuccess("T < \"FOO\";", new EidosValue_Logical(false));
	AssertScriptSuccess("\"FOO\" < T;", new EidosValue_Logical(true));
	AssertScriptSuccess("T < \"XYZZY\";", new EidosValue_Logical(true));
	AssertScriptSuccess("\"XYZZY\" < T;", new EidosValue_Logical(false));
	AssertScriptSuccess("5 < -10;", new EidosValue_Logical(false));
	AssertScriptSuccess("-10 < 5;", new EidosValue_Logical(true));
	AssertScriptSuccess("5.0 < -10;", new EidosValue_Logical(false));
	AssertScriptSuccess("-10 < 5.0;", new EidosValue_Logical(true));
	AssertScriptSuccess("5 < -10.0;", new EidosValue_Logical(false));
	AssertScriptSuccess("-10.0 < 5;", new EidosValue_Logical(true));
	AssertScriptSuccess("\"foo\" < \"bar\";", new EidosValue_Logical(false));
	AssertScriptSuccess("\"bar\" < \"foo\";", new EidosValue_Logical(true));
	AssertScriptSuccess("120 < \"10\";", new EidosValue_Logical(false));
	AssertScriptSuccess("10 < \"120\";", new EidosValue_Logical(true));
	AssertScriptSuccess("120 < \"15\";", new EidosValue_Logical(true));
	AssertScriptSuccess("15 < \"120\";", new EidosValue_Logical(false));
	AssertScriptRaise("_Test(9) < 5");
	AssertScriptRaise("5 < _Test(9)");
	AssertScriptSuccess("NULL < 5;", new EidosValue_Logical());
	AssertScriptSuccess("NULL < 5.0;", new EidosValue_Logical());
	AssertScriptSuccess("NULL < \"foo\";", new EidosValue_Logical());
	AssertScriptSuccess("5 < NULL;", new EidosValue_Logical());
	AssertScriptSuccess("5.0 < NULL;", new EidosValue_Logical());
	AssertScriptSuccess("\"foo\" < NULL;", new EidosValue_Logical());
	
	// operator >=
	
	// operator <=
	
	// operator ==
	
	// operator !=
	
	// operator :
	
	// operator ^
	
	// operator &
	
	// operator |
	
	// operator !
	
	
	// ************************************************************************************
	//
	//	Keyword tests
	//
	
	// if
	
	// if-else
	
	// do
	
	// while
	
	// for and in
	
	// next
	
	// break
	
	// return
	
	
	// ************************************************************************************
	//
	//	Function tests
	//
	
	// abs()
	
	// acos()
	
	// asin()
	
	// atan()
	
	// atan2()
	
	// ceil()
	
	// cos()
	
	// exp()
	
	// floor()
	
	// isFinite()
	
	// isInfinite()
	
	// isNaN()
	
	// log()
	
	// log10()
	
	// log2()
	
	// product()
	
	// round()
	
	// sin()
	
	// sqrt()
	
	// sum()
	
	// tan()
	
	// trunc()

	// max()
	
	// mean()
	
	// min()
	
	// range()
	
	// sd()
	
	// c()
	
	// float()
	
	// integer()
	
	// logical()
	
	// object()
	
	// rbinom()
	AssertScriptSuccess("rbinom(0, 10, 0.5);", new EidosValue_Int_vector());
	AssertScriptSuccess("rbinom(3, 10, 0.0);", new EidosValue_Int_vector(0, 0, 0));
	AssertScriptSuccess("rbinom(3, 10, 1.0);", new EidosValue_Int_vector(10, 10, 10));
	AssertScriptSuccess("rbinom(3, 0, 0.0);", new EidosValue_Int_vector(0, 0, 0));
	AssertScriptSuccess("rbinom(3, 0, 1.0);", new EidosValue_Int_vector(0, 0, 0));
	AssertScriptSuccess("setSeed(1); rbinom(5, 10, 0.5);", new EidosValue_Int_vector(4, 8, 5, 3, 4));
	AssertScriptSuccess("setSeed(2); rbinom(5, 10, 0.5);", new EidosValue_Int_vector(7, 6, 3, 6, 3));
	AssertScriptSuccess("setSeed(3); rbinom(5, 1000, 0.01);", new EidosValue_Int_vector(11, 16, 10, 14, 10));
	AssertScriptSuccess("setSeed(4); rbinom(5, 1000, 0.99);", new EidosValue_Int_vector(992, 990, 995, 991, 995));
	AssertScriptSuccess("setSeed(5); rbinom(3, 100, c(0.1, 0.5, 0.9));", new EidosValue_Int_vector(7, 50, 87));
	AssertScriptSuccess("setSeed(6); rbinom(3, c(10, 30, 50), 0.5);", new EidosValue_Int_vector(6, 12, 26));
	AssertScriptRaise("rbinom(-1, 10, 0.5);");
	AssertScriptRaise("rbinom(3, -1, 0.5);");
	AssertScriptRaise("rbinom(3, 10, -0.1);");
	AssertScriptRaise("rbinom(3, 10, 1.1);");
	AssertScriptRaise("rbinom(3, 10, c(0.1, 0.2));");
	AssertScriptRaise("rbinom(3, c(10, 12), 0.5);");
	
	// rep()
	
	// repEach()
	
	// rexp()
	AssertScriptSuccess("rexp(0);", new EidosValue_Float_vector());
	AssertScriptSuccess("setSeed(1); (rexp(3) - c(0.206919, 3.01675, 0.788416)) < 0.000001;", new EidosValue_Logical(true, true, true));
	AssertScriptSuccess("setSeed(2); (rexp(3, 0.1) - c(20.7, 12.2, 0.9)) < 0.1;", new EidosValue_Logical(true, true, true));
	AssertScriptSuccess("setSeed(3); (rexp(3, 0.00001) - c(95364.3, 307170.0, 74334.9)) < 0.1;", new EidosValue_Logical(true, true, true));
	AssertScriptSuccess("setSeed(4); (rexp(3, c(0.1, 0.01, 0.001)) - c(2.8, 64.6, 58.8)) < 0.1;", new EidosValue_Logical(true, true, true));
	AssertScriptRaise("rexp(-1);");
	AssertScriptRaise("rexp(3, 0.0);");
	AssertScriptRaise("rexp(3, -1.0);");
	AssertScriptRaise("rexp(3, c(0.1, 0.2));");
	
	// rnorm()
	AssertScriptSuccess("rnorm(0);", new EidosValue_Float_vector());
	AssertScriptSuccess("rnorm(3, 0, 0);", new EidosValue_Float_vector(0.0, 0.0, 0.0));
	AssertScriptSuccess("rnorm(3, 1, 0);", new EidosValue_Float_vector(1.0, 1.0, 1.0));
	AssertScriptSuccess("setSeed(1); (rnorm(2) - c(-0.785386, 0.132009)) < 0.000001;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(2); (rnorm(2, 10.0) - c(10.38, 10.26)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(3); (rnorm(2, 10.0, 100.0) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(4); (rnorm(2, c(-10, 10), 100.0) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(5); (rnorm(2, 10.0, c(0.1, 10)) - c(59.92, 95.35)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptRaise("rnorm(-1);");
	AssertScriptRaise("rnorm(1, 0, -1);");
	AssertScriptRaise("rnorm(2, c(-10, 10, 1), 100.0);");
	AssertScriptRaise("rnorm(2, 10.0, c(0.1, 10, 1));");
	
	// rpois()
	AssertScriptSuccess("rpois(0, 1.0);", new EidosValue_Int_vector());
	AssertScriptSuccess("setSeed(1); rpois(5, 1.0);", new EidosValue_Int_vector(0, 2, 0, 1, 1));
	AssertScriptSuccess("setSeed(2); rpois(5, 0.2);", new EidosValue_Int_vector(1, 0, 0, 0, 0));
	AssertScriptSuccess("setSeed(3); rpois(5, 10000);", new EidosValue_Int_vector(10205, 10177, 10094, 10227, 9875));
	AssertScriptSuccess("setSeed(4); rpois(5, c(1, 10, 100, 1000, 10000));", new EidosValue_Int_vector(0, 8, 97, 994, 9911));
	AssertScriptRaise("rpois(-1, 1.0);");
	AssertScriptRaise("rpois(0, 0.0);");
	AssertScriptRaise("setSeed(4); rpois(5, c(1, 10, 100, 1000));");
	
	// runif()
	AssertScriptSuccess("runif(0);", new EidosValue_Float_vector());
	AssertScriptSuccess("runif(3, 0, 0);", new EidosValue_Float_vector(0.0, 0.0, 0.0));
	AssertScriptSuccess("runif(3, 1, 1);", new EidosValue_Float_vector(1.0, 1.0, 1.0));
	AssertScriptSuccess("setSeed(1); (runif(2) - c(0.186915, 0.951040)) < 0.000001;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(2); (runif(2, 0.5) - c(0.93, 0.85)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(3); (runif(2, 10.0, 100.0) - c(65.31, 95.82)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(4); (runif(2, c(-100, 1), 10.0) - c(-72.52, 5.28)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptSuccess("setSeed(5); (runif(2, -10.0, c(1, 1000)) - c(-8.37, 688.97)) < 0.01;", new EidosValue_Logical(true, true));
	AssertScriptRaise("runif(-1);");
	AssertScriptRaise("runif(1, 0, -1);");
	AssertScriptRaise("runif(2, c(-10, 10, 1), 100.0);");
	AssertScriptRaise("runif(2, -10.0, c(0.1, 10, 1));");
	
	// sample()
	
	// seq()
	AssertScriptSuccess("seq(1, 5);", new EidosValue_Int_vector(1, 2, 3, 4, 5));
	AssertScriptSuccess("seq(5, 1);", new EidosValue_Int_vector(5, 4, 3, 2, 1));
	AssertScriptSuccess("seq(1.1, 5);", new EidosValue_Float_vector(1.1, 2.1, 3.1, 4.1));
	AssertScriptSuccess("seq(1, 5.1);", new EidosValue_Float_vector(1, 2, 3, 4, 5));
	AssertScriptSuccess("seq(1, 10, 2);", new EidosValue_Int_vector(1, 3, 5, 7, 9));
	AssertScriptRaise("seq(1, 10, -2);");
	AssertScriptSuccess("seq(10, 1, -2);", new EidosValue_Int_vector(10, 8, 6, 4, 2));
	AssertScriptSuccess("(seq(1, 2, 0.2) - c(1, 1.2, 1.4, 1.6, 1.8, 2.0)) < 0.000000001;", new EidosValue_Logical(true, true, true, true, true, true));
	AssertScriptRaise("seq(1, 2, -0.2);");
	AssertScriptSuccess("(seq(2, 1, -0.2) - c(2.0, 1.8, 1.6, 1.4, 1.2, 1)) < 0.000000001;", new EidosValue_Logical(true, true, true, true, true, true));
	AssertScriptRaise("seq(\"foo\", 2, 1);");
	AssertScriptRaise("seq(1, \"foo\", 2);");
	AssertScriptRaise("seq(2, 1, \"foo\");");
	AssertScriptRaise("seq(T, 2, 1);");
	AssertScriptRaise("seq(1, T, 2);");
	AssertScriptRaise("seq(2, 1, T);");
		// FIXME test with NULL
	
	// seqAlong()
	
	// string()
	
	// all()
	
	// any()
	
	// cat()
	
	// ifelse()
	
	// nchar()
	
	// paste()
	
	// print()
	
	// rev()
	AssertScriptSuccess("rev(6:10);", new EidosValue_Int_vector(10,9,8,7,6));
	AssertScriptSuccess("rev(-(6:10));", new EidosValue_Int_vector(-10,-9,-8,-7,-6));
	AssertScriptSuccess("rev(c(\"foo\",\"bar\",\"baz\"));", new EidosValue_String("baz","bar","foo"));
	AssertScriptSuccess("rev(-1);", new EidosValue_Int_singleton_const(-1));
	AssertScriptSuccess("rev(1.0);", new EidosValue_Float_singleton_const(1));
	AssertScriptSuccess("rev(\"foo\");", new EidosValue_String("foo"));
	AssertScriptSuccess("rev(6.0:10);", new EidosValue_Float_vector(10,9,8,7,6));
	AssertScriptSuccess("rev(c(T,T,T,F));", new EidosValue_Logical(false, true, true, true));
	
	// size()
	
	// sort()
	
	// sortBy()
	
	// str()
	
	// strsplit()
	
	// substr()
	AssertScriptSuccess("substr(string(0), 1);", new EidosValue_String());
	AssertScriptSuccess("substr(string(0), 1, 2);", new EidosValue_String());
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1);", new EidosValue_String("oo", "ar", "oobaz"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1, 10000);", new EidosValue_String("oo", "ar", "oobaz"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1, 1);", new EidosValue_String("o", "a", "o"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1, 2);", new EidosValue_String("oo", "ar", "oo"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1, 3);", new EidosValue_String("oo", "ar", "oob"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, c(1, 2, 3));", new EidosValue_String("oo", "r", "baz"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1, c(1, 2, 3));", new EidosValue_String("o", "ar", "oob"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, c(1, 2, 3), c(1, 2, 3));", new EidosValue_String("o", "r", "b"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, c(1, 2, 3), c(2, 4, 6));", new EidosValue_String("oo", "r", "baz"));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1, 0);", new EidosValue_String("", "", ""));
	AssertScriptSuccess("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, -100, 1);", new EidosValue_String("fo", "ba", "fo"));
	AssertScriptRaise("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, 1, c(2, 4));");
	AssertScriptRaise("x=c(\"foo\",\"bar\",\"foobaz\"); substr(x, c(1, 2), 4);");
	
	// unique()
	
	// which()
	
	// whichMax()
	
	// whichMin()
	
	// asFloat()
	AssertScriptSuccess("asFloat(-1:3);", new EidosValue_Float_vector(-1,0,1,2,3));
	AssertScriptSuccess("asFloat(-1.0:3);", new EidosValue_Float_vector(-1,0,1,2,3));
	AssertScriptSuccess("asFloat(c(T,F,T,F));", new EidosValue_Float_vector(1,0,1,0));
	AssertScriptSuccess("asFloat(c(\"1\",\"2\",\"3\"));", new EidosValue_Float_vector(1,2,3));
	AssertScriptSuccess("asFloat(\"foo\");", new EidosValue_Float_singleton_const(0)); // FIXME should this be an error?
	
	// asInteger()
	AssertScriptSuccess("asInteger(-1:3);", new EidosValue_Int_vector(-1,0,1,2,3));
	AssertScriptSuccess("asInteger(-1.0:3);", new EidosValue_Int_vector(-1,0,1,2,3));
	AssertScriptSuccess("asInteger(c(T,F,T,F));", new EidosValue_Int_vector(1,0,1,0));
	AssertScriptSuccess("asInteger(c(\"1\",\"2\",\"3\"));", new EidosValue_Int_vector(1,2,3));
	AssertScriptSuccess("asInteger(\"foo\");", new EidosValue_Int_singleton_const(0)); // FIXME should this be an error?
	
	// asLogical()
	AssertScriptSuccess("asLogical(-1:3);", new EidosValue_Logical(true,false,true,true,true));
	AssertScriptSuccess("asLogical(-1.0:3);", new EidosValue_Logical(true,false,true,true,true));
	AssertScriptSuccess("asLogical(c(T,F,T,F));", new EidosValue_Logical(true,false,true,false));
	AssertScriptSuccess("asLogical(c(\"foo\",\"bar\",\"\"));", new EidosValue_Logical(true,true,false));
	
	// asString()
	AssertScriptSuccess("asString(-1:3);", new EidosValue_String("-1","0","1","2","3"));
	AssertScriptSuccess("asString(-1.0:3);", new EidosValue_String("-1","0","1","2","3"));
	AssertScriptSuccess("asString(c(T,F,T,F));", new EidosValue_String("T","F","T","F"));
	AssertScriptSuccess("asString(c(\"1\",\"2\",\"3\"));", new EidosValue_String("1","2","3"));
	
	// element()
	
	// isFloat()
	
	// isInteger()
	
	// isLogical()
	
	// isNULL()
	
	// isObject()
	
	// isString()
	
	// type()
	
	// date()
	
	// executeLambda()
	
	// function()
	
	// globals()
	
	// help()
	
	// license()
	
	// rm()
	
	// setSeed()
	
	// getSeed()
	
	// stop()
	
	// time()
	
	// version()
	
	
	// ************************************************************************************
	//
    //	Print a summary of test results
	//
    std::cerr << endl;
    if (gTestFailureCount)
        std::cerr << "\e[31mFAILURE\e[0m count: " << gTestFailureCount << endl;
    std::cerr << "\e[32mSUCCESS\e[0m count: " << gTestSuccessCount << endl;
	
	// If we ran tests, the random number seed has been set; let's set it back to a good seed value
	EidosInitializeRNGFromSeed(EidosGenerateSeedFromPIDAndTime());
	
	std::cerr << "INT_MAX == " << INT_MAX << endl;
}































































