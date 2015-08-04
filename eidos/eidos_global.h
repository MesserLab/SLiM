//
//  eidos_global.h
//  Eidos
//
//  Created by Ben Haller on 6/28/15.
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


#ifndef __Eidos__eidos_global__
#define __Eidos__eidos_global__

#include <stdio.h>
#include <iostream>
#include <sstream>


// Information on the Context within which Eidos is running (if any).  This is basically a way to let the Context
// customize the version and license information printed by Eidos.

extern std::string gEidosContextVersion;
extern std::string gEidosContextLicense;


// If EIDOS_TERMINATE_THROWS is not defined, << eidos_terminate causes a call to exit().  In that mode, output to the
// main Eidos outstream goes to cout, and error and termination output goes to cerr.  The other mode has EIDOS_TERMINATE_THROWS
// defined.  In that mode, we use a global ostringstream to capture all output to both the output and error streams.
// This stream should get emptied out after every call in to Eidos, so a single stream can be safely used by multiple
// Eidos contexts (as long as we do not multithread).  We also have a special stream for termination messages in that mode,
// since they need to be reported to the user in a prominent way.
#if defined(EIDOS_TERMINATE_THROWS)

extern std::ostringstream gEidosOut;
extern std::ostringstream gEidosTermination;
#define EIDOS_OUTSTREAM		(gEidosOut)
#define EIDOS_ERRSTREAM		(gEidosOut)
#define EIDOS_TERMINATION	(gEidosTermination)

#else

#define EIDOS_OUTSTREAM		(std::cout)
#define EIDOS_ERRSTREAM		(std::cerr)
#define EIDOS_TERMINATION	(std::cerr)

#endif


// the part of the input file that caused an error; set by the parsing code (SetErrorPositionFromCurrentToken
// in particular) and used by EidosTextView to highlight the token or text that caused the error
extern int gEidosCharacterStartOfParseError, gEidosCharacterEndOfParseError;


// Print a demangled stack backtrace of the caller function to FILE* out; see eidos_global.cpp for credits and comments.
void eidos_print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63);


// This little class is used as a stream manipulator that causes termination with EXIT_FAILURE, optionally
// with a backtrace.  This is nice since it lets us log and terminate in a single line of code.  It also allows
// a GUI to intercept the exit() call and do something more graceful with it.
class eidos_terminate
{
public:
	bool print_backtrace_ = false;
	
	eidos_terminate(void) = default;														// default constructor, no backtrace
	eidos_terminate(bool p_print_backtrace) : print_backtrace_(p_print_backtrace) {};	// optionally request a backtrace
};

std::ostream& operator<<(std::ostream& p_out, const eidos_terminate &p_terminator);	// note this returns void, not std::ostream&; that is deliberate

// Get the message from the last raise out of gEidosTermination, optionally with newlines trimmed from the ends
std::string EidosGetTrimmedRaiseMessage(void);
std::string EidosGetUntrimmedRaiseMessage(void);


// Resolve a leading ~ in a filesystem path to the user's home directory
std::string EidosResolvedPath(const std::string p_path);


//
//	Global std::string objects.  This is kind of gross, but there are several rationales for it.  First of all, it makes
//	a speed difference; converting a C string to a std::string is done every time it is hit in the code (C++ does not
//	treat that as a constant expression and cache it for you, at least with the current generation of compilers).  The
//	conversion is surprisingly slow; it has shown up repeatedly in profiles I have done.  Second, there is the issue of
//	uniqueness; many of these strings occur in multiple places in the code, and a typo in one of those multiple occurrences
//	would cause a bug that would be very difficult to find.  If multiple places in the code intend to refer to the same
//	conceptual string, and rely on those references being the same, then a shared constant should be used.  So... oh well.
//

typedef int EidosGlobalStringID;

EidosGlobalStringID EidosGlobalStringIDForString(const std::string &p_string);		// takes any string
const std::string &StringForEidosGlobalStringID(EidosGlobalStringID p_string_id);		// returns the uniqued global string

void Eidos_RegisterStringForGlobalID(const std::string &p_string, EidosGlobalStringID p_string_id);
void Eidos_RegisterGlobalStringsAndIDs(void);


extern const std::string gEidosStr_empty_string;
extern const std::string gEidosStr_space_string;

extern const std::string gEidosStr_function;
extern const std::string gEidosStr_method;
extern const std::string gEidosStr_executeLambda;
extern const std::string gEidosStr_globals;

extern const std::string gEidosStr_if;
extern const std::string gEidosStr_else;
extern const std::string gEidosStr_do;
extern const std::string gEidosStr_while;
extern const std::string gEidosStr_for;
extern const std::string gEidosStr_in;
extern const std::string gEidosStr_next;
extern const std::string gEidosStr_break;
extern const std::string gEidosStr_return;

extern const std::string gEidosStr_T;
extern const std::string gEidosStr_F;
extern const std::string gEidosStr_NULL;
extern const std::string gEidosStr_PI;
extern const std::string gEidosStr_E;
extern const std::string gEidosStr_INF;
extern const std::string gEidosStr_NAN;

extern const std::string gEidosStr_void;
extern const std::string gEidosStr_logical;
extern const std::string gEidosStr_string;
extern const std::string gEidosStr_integer;
extern const std::string gEidosStr_float;
extern const std::string gEidosStr_object;
extern const std::string gEidosStr_numeric;

extern const std::string gEidosStr_size;
extern const std::string gEidosStr_property;
extern const std::string gEidosStr_str;

extern const std::string gEidosStr_GetPropertyOfElements;
extern const std::string gEidosStr_ExecuteMethod;
extern const std::string gEidosStr_lessThanSign;
extern const std::string gEidosStr_greaterThanSign;
extern const std::string gEidosStr_undefined;

extern const std::string gEidosStr__TestElement;
extern const std::string gEidosStr__yolk;
extern const std::string gEidosStr__cubicYolk;


// Not all global strings have a EidosGlobalStringID; basically just ones that we want to scan and pre-cache in the tree,
// such as property and method names, as well as initialize...() function names (since signatures can't be cached for them).
enum _EidosGlobalStringID : int {
	gEidosID_none = 0,
	gEidosID_method,
	gEidosID_size,
	gEidosID_property,
	gEidosID_str,
	
	gEidosID__TestElement,
	gEidosID__yolk,
	gEidosID__cubicYolk,
	
	gEidosID_LastEntry		// IDs added by the Context should start here
};


#endif /* defined(__Eidos__eidos_global__) */



















































