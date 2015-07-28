//
//  eidos_global.cpp
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


#include "eidos_global.h"

#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <ctype.h>
#include <stdexcept>
#include <string>
#include <map>


// Information on the Context within which Eidos is running (if any).
std::string gEidosContextVersion;
std::string gEidosContextLicense;


// the part of the input file that caused an error; set by the parsing code (SetErrorPositionFromCurrentToken
// in particular) and used by EidosTextView to highlight the token or text that caused the error
int gEidosCharacterStartOfParseError = -1, gEidosCharacterEndOfParseError = -1;


// define string streams used for output when EIDOS_TERMINATE_THROWS is defined; otherwise, terminates call exit()
#if defined(EIDOS_TERMINATE_THROWS)

std::ostringstream gEidosOut;
std::ostringstream gEidosTermination;
bool gEidosTerminated;

#else

#endif


/** Print a demangled stack backtrace of the caller function to FILE* out. */
void eidos_print_stacktrace(FILE *out, unsigned int max_frames)
{
	fprintf(out, "stack trace:\n");
	
	// storage array for stack trace address data
	void* addrlist[max_frames+1];
	
	// retrieve current stack addresses
	int addrlen = backtrace(addrlist, static_cast<int>(sizeof(addrlist) / sizeof(void*)));
	
	if (addrlen == 0)
	{
		fprintf(out, "  <empty, possibly corrupt>\n");
		return;
	}
	
	// resolve addresses into strings containing "filename(function+address)",
	// this array must be free()-ed
	char** symbollist = backtrace_symbols(addrlist, addrlen);
	
	// allocate string which will be filled with the demangled function name
	size_t funcnamesize = 256;
	char* funcname = (char*)malloc(funcnamesize);
	
	// iterate over the returned symbol lines. skip the first, it is the
	// address of this function.
	for (int i = 1; i < addrlen; i++)
	{
		char *begin_name = 0, *end_name = 0, *begin_offset = 0, *end_offset = 0;
		
		// find parentheses and +address offset surrounding the mangled name:
		// ./module(function+0x15c) [0x8048a6d]
		for (char *p = symbollist[i]; *p; ++p)
		{
			if (*p == '(')
				begin_name = p;
			else if (*p == '+')
				begin_offset = p;
			else if (*p == ')' && begin_offset)
			{
				end_offset = p;
				break;
			}
		}
		
		// BCH 24 Dec 2014: backtrace_symbols() on OS X seems to return strings in a different, non-standard format.
		// Added this code in an attempt to parse that format.  No doubt it could be done more cleanly.  :->
		if (!(begin_name && begin_offset && end_offset
			  && begin_name < begin_offset))
		{
			enum class ParseState {
				kInWhitespace1 = 1,
				kInLineNumber,
				kInWhitespace2,
				kInPackageName,
				kInWhitespace3,
				kInAddress,
				kInWhitespace4,
				kInFunction,
				kInWhitespace5,
				kInPlus,
				kInWhitespace6,
				kInOffset,
				kInOverrun
			};
			ParseState parse_state = ParseState::kInWhitespace1;
			char *p;
			
			for (p = symbollist[i]; *p; ++p)
			{
				switch (parse_state)
				{
					case ParseState::kInWhitespace1:	if (!isspace(*p)) parse_state = ParseState::kInLineNumber;	break;
					case ParseState::kInLineNumber:		if (isspace(*p)) parse_state = ParseState::kInWhitespace2;	break;
					case ParseState::kInWhitespace2:	if (!isspace(*p)) parse_state = ParseState::kInPackageName;	break;
					case ParseState::kInPackageName:	if (isspace(*p)) parse_state = ParseState::kInWhitespace3;	break;
					case ParseState::kInWhitespace3:	if (!isspace(*p)) parse_state = ParseState::kInAddress;		break;
					case ParseState::kInAddress:		if (isspace(*p)) parse_state = ParseState::kInWhitespace4;	break;
					case ParseState::kInWhitespace4:
						if (!isspace(*p))
						{
							parse_state = ParseState::kInFunction;
							begin_name = p - 1;
						}
						break;
					case ParseState::kInFunction:
						if (isspace(*p))
						{
							parse_state = ParseState::kInWhitespace5;
							end_name = p;
						}
						break;
					case ParseState::kInWhitespace5:	if (!isspace(*p)) parse_state = ParseState::kInPlus;		break;
					case ParseState::kInPlus:			if (isspace(*p)) parse_state = ParseState::kInWhitespace6;	break;
					case ParseState::kInWhitespace6:
						if (!isspace(*p))
						{
							parse_state = ParseState::kInOffset;
							begin_offset = p - 1;
						}
						break;
					case ParseState::kInOffset:
						if (isspace(*p))
						{
							parse_state = ParseState::kInOverrun;
							end_offset = p;
						}
						break;
					case ParseState::kInOverrun:
						break;
				}
			}
			
			if (parse_state == ParseState::kInOffset && !end_offset)
				end_offset = p;
		}
		
		if (begin_name && begin_offset && end_offset
			&& begin_name < begin_offset)
		{
			*begin_name++ = '\0';
			if (end_name)
				*end_name = '\0';
			*begin_offset++ = '\0';
			*end_offset = '\0';
			
			// mangled name is now in [begin_name, begin_offset) and caller
			// offset in [begin_offset, end_offset). now apply __cxa_demangle():
			
			int status;
			char *ret = abi::__cxa_demangle(begin_name, funcname, &funcnamesize, &status);
			
			if (status == 0)
			{
				funcname = ret; // use possibly realloc()-ed string
				fprintf(out, "  %s : %s + %s\n", symbollist[i], funcname, begin_offset);
			}
			else
			{
				// demangling failed. Output function name as a C function with
				// no arguments.
				fprintf(out, "  %s : %s() + %s\n", symbollist[i], begin_name, begin_offset);
			}
		}
		else
		{
			// couldn't parse the line? print the whole line.
			fprintf(out, "URF:  %s\n", symbollist[i]);
		}
	}
	
	free(funcname);
	free(symbollist);
	
	fflush(out);
}

std::ostream& operator<<(std::ostream& p_out, const eidos_terminate &p_terminator)
{
	p_out << std::endl;
	
	p_out.flush();
	
	if (p_terminator.print_backtrace_)
	{
		if (p_out == EIDOS_OUTSTREAM)
			eidos_print_stacktrace(stdout);
		else
			eidos_print_stacktrace(stderr);
	}
	
#if defined(EIDOS_TERMINATE_THROWS)
	// In this case, eidos_terminate() throws an exception that gets caught by the Context.  That invalidates the simulation object, and
	// causes the Context to display an error message and ends the simulation run, but it does not terminate the app.
	throw std::runtime_error("A runtime error occurred in Eidos");
#else
	// In this case, eidos_terminate() does in fact terminate; this is appropriate when errors are simply fatal and there is no UI.
	exit(EXIT_FAILURE);
#endif
	
	return p_out;
}

std::string EidosGetTrimmedRaiseMessage(void)
{
#if defined(EIDOS_TERMINATE_THROWS)
	std::string terminationMessage = gEidosTermination.str();
	
	gEidosTermination.clear();
	gEidosTermination.str(gStr_empty_string);
	
	// trim off newlines at the end of the raise string
	size_t endpos = terminationMessage.find_last_not_of("\n\r");
	if (std::string::npos != endpos)
		terminationMessage = terminationMessage.substr(0, endpos + 1);
	
	return terminationMessage;
#else
	return gStr_empty_string;
#endif
}

std::string EidosGetUntrimmedRaiseMessage(void)
{
#if defined(EIDOS_TERMINATE_THROWS)
	std::string terminationMessage = gEidosTermination.str();
	
	gEidosTermination.clear();
	gEidosTermination.str(gStr_empty_string);
	
	return terminationMessage;
#else
	return gStr_empty_string;
#endif
}


//	Global std::string objects.
const std::string gStr_empty_string = "";
const std::string gStr_space_string = " ";

// mostly function names used in multiple places
const std::string gStr_function = "function";
const std::string gStr_method = "method";
const std::string gStr_executeLambda = "executeLambda";
const std::string gStr_globals = "globals";
const std::string gStr_Path = "Path";

// mostly language keywords
const std::string gStr_if = "if";
const std::string gStr_else = "else";
const std::string gStr_do = "do";
const std::string gStr_while = "while";
const std::string gStr_for = "for";
const std::string gStr_in = "in";
const std::string gStr_next = "next";
const std::string gStr_break = "break";
const std::string gStr_return = "return";

// mostly Eidos global constants
const std::string gStr_T = "T";
const std::string gStr_F = "F";
const std::string gStr_NULL = "NULL";
const std::string gStr_PI = "PI";
const std::string gStr_E = "E";
const std::string gStr_INF = "INF";
const std::string gStr_NAN = "NAN";

// mostly Eidos type names
const std::string gStr_void = "void";
const std::string gStr_logical = "logical";
const std::string gStr_string = "string";
const std::string gStr_integer = "integer";
const std::string gStr_float = "float";
const std::string gStr_object = "object";
const std::string gStr_numeric = "numeric";

// Eidos function names, property names, and method names
const std::string gStr_size = "size";
const std::string gStr_type = "type";
const std::string gStr_property = "property";
const std::string gStr_str = "str";
const std::string gStr_path = "path";
const std::string gStr_files = "files";
const std::string gStr_readFile = "readFile";
const std::string gStr_writeFile = "writeFile";

// other miscellaneous strings
const std::string gStr_GetValueForMemberOfElements = "GetValueForMemberOfElements";
const std::string gStr_ExecuteMethod = "ExecuteMethod";
const std::string gStr_lessThanSign = "<";
const std::string gStr_greaterThanSign = ">";
const std::string gStr_undefined = "undefined";


static std::map<const std::string, EidosGlobalStringID> gStringToID;
static std::map<EidosGlobalStringID, const std::string *> gIDToString;

void Eidos_RegisterStringForGlobalID(const std::string &p_string, EidosGlobalStringID p_string_id)
{
	if (gStringToID.find(p_string) != gStringToID.end())
		EIDOS_TERMINATION << "ERROR (Eidos_RegisterStringForGlobalID): string " << p_string << " has already been registered." << eidos_terminate();
	
	if (gIDToString.find(p_string_id) != gIDToString.end())
		EIDOS_TERMINATION << "ERROR (Eidos_RegisterStringForGlobalID): id " << p_string_id << " has already been registered." << eidos_terminate();
	
	gStringToID[p_string] = p_string_id;
	gIDToString[p_string_id] = &p_string;
}

void Eidos_RegisterGlobalStringsAndIDs(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		Eidos_RegisterStringForGlobalID(gStr_method, gID_method);
		Eidos_RegisterStringForGlobalID(gStr_Path, gID_Path);
		Eidos_RegisterStringForGlobalID(gStr_size, gID_size);
		Eidos_RegisterStringForGlobalID(gStr_type, gID_type);
		Eidos_RegisterStringForGlobalID(gStr_property, gID_property);
		Eidos_RegisterStringForGlobalID(gStr_str, gID_str);
		Eidos_RegisterStringForGlobalID(gStr_path, gID_path);
		Eidos_RegisterStringForGlobalID(gStr_files, gID_files);
		Eidos_RegisterStringForGlobalID(gStr_readFile, gID_readFile);
		Eidos_RegisterStringForGlobalID(gStr_writeFile, gID_writeFile);
	}
}

EidosGlobalStringID EidosGlobalStringIDForString(const std::string &p_string)
{
	//std::cerr << "EidosGlobalStringIDForString: " << p_string << std::endl;
	auto found_iter = gStringToID.find(p_string);
	
	if (found_iter == gStringToID.end())
		return gID_none;
	else
		return found_iter->second;
}

const std::string &StringForEidosGlobalStringID(EidosGlobalStringID p_string_id)
{
	//std::cerr << "StringForEidosGlobalStringID: " << p_string_id << std::endl;
	auto found_iter = gIDToString.find(p_string_id);
	
	if (found_iter == gIDToString.end())
		return gStr_undefined;
	else
		return *(found_iter->second);
}


















































