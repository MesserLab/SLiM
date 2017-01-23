//
//  eidos_global.cpp
//  Eidos
//
//  Created by Ben Haller on 6/28/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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


#include "eidos_global.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_interpreter.h"
#include "eidos_object_pool.h"
#include "eidos_ast_node.h"

#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <ctype.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <pwd.h>
#include <unistd.h>
#include <algorithm>
#include "string.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <memory>


bool eidos_do_memory_checks = true;

EidosSymbolTable *gEidosConstantsSymbolTable = nullptr;


void Eidos_WarmUp(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		// Make the shared EidosValue pool
		size_t maxEidosValueSize = sizeof(EidosValue_NULL);
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Logical));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Logical_const));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_String));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_String_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_String_singleton));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Int));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Int_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Int_singleton));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Float));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Float_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Float_singleton));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Object));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Object_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Object_singleton));
		
		//std::cout << "maxEidosValueSize == " << maxEidosValueSize << std::endl;
		gEidosValuePool = new EidosObjectPool(maxEidosValueSize);
		
		// Make the shared EidosASTNode pool
		gEidosASTNodePool = new EidosObjectPool(sizeof(EidosASTNode));
		
		// Allocate global permanents
		gStaticEidosValueNULL = EidosValue_NULL::Static_EidosValue_NULL();
		gStaticEidosValueNULLInvisible = EidosValue_NULL::Static_EidosValue_NULL_Invisible();
		
		gStaticEidosValue_Logical_ZeroVec = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
		gStaticEidosValue_Integer_ZeroVec = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
		gStaticEidosValue_Float_ZeroVec = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		gStaticEidosValue_String_ZeroVec = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
		gStaticEidosValue_Object_ZeroVec = EidosValue_Object_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidos_UndefinedClassObject));
		
		gStaticEidosValue_LogicalT = EidosValue_Logical_const::Static_EidosValue_Logical_T();
		gStaticEidosValue_LogicalF = EidosValue_Logical_const::Static_EidosValue_Logical_F();
		
		gStaticEidosValue_Integer0 = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
		gStaticEidosValue_Integer1 = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1));
		
		gStaticEidosValue_Float0 = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.0));
		gStaticEidosValue_Float0Point5 = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.5));
		gStaticEidosValue_Float1 = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1.0));
		
		gStaticEidosValue_StringEmpty = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		gStaticEidosValue_StringSpace = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(" "));
		gStaticEidosValue_StringAsterisk = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("*"));
		
		// Register global strings and IDs
		Eidos_RegisterGlobalStringsAndIDs();
		
		// Set up the built-in function map, which is immutable
		EidosInterpreter::CacheBuiltInFunctionMap();
		
		// Set up the symbol table for Eidos constants
		gEidosConstantsSymbolTable = new EidosSymbolTable(EidosSymbolTableType::kEidosIntrinsicConstantsTable, nullptr);
	}
}

void Eidos_DefineConstantsFromCommandLine(std::vector<std::string> p_constants)
{
	// We want to throw exceptions, even in SLiM, so that we can catch them here
	bool save_throws = gEidosTerminateThrows;
	
	gEidosTerminateThrows = true;
	
	for (std::string &constant : p_constants)
	{
		// Each constant must be in the form x=y, where x is a valid identifier and y is a valid singleton
		// Eidos value (integer, float, logical, or string).  Of course this could be broadened later.
		// We parse the assignment using EidosScript, and work with the resulting AST, for generality.
		EidosScript script(constant);
		bool malformed = false;
		
		try
		{
			script.SetFinalSemicolonOptional(true);
			script.Tokenize();
			script.ParseInterpreterBlockToAST();
		}
		catch (...)
		{
			malformed = true;
		}
		
		if (!malformed)
		{
			//script.PrintTokens(std::cout);
			//script.PrintAST(std::cout);
			
			const EidosASTNode *AST = script.AST();
			
			if (AST && (AST->token_->token_type_ == EidosTokenType::kTokenInterpreterBlock) && (AST->children_.size() == 1))
			{
				const EidosASTNode *top_node = AST->children_[0];
				
				if (top_node && (top_node->token_->token_type_ == EidosTokenType::kTokenAssign) && (top_node->children_.size() == 2))
				{
					const EidosASTNode *left_node = top_node->children_[0];
					
					if (left_node && (left_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (left_node->children_.size() == 0))
					{
						std::string symbol_name = left_node->token_->token_string_;
						bool good_symbol = true;
						
						// Eidos constants are reserved
						if ((symbol_name == "T") || (symbol_name == "F") || (symbol_name == "NULL") || (symbol_name == "PI") || (symbol_name == "E") || (symbol_name == "INF") || (symbol_name == "NAN"))
							good_symbol = false;
						
						// Eidos keywords are reserved (probably won't reach here anyway)
						if ((symbol_name == "if") || (symbol_name == "else") || (symbol_name == "do") || (symbol_name == "while") || (symbol_name == "for") || (symbol_name == "in") || (symbol_name == "next") || (symbol_name == "break") || (symbol_name == "return"))
							good_symbol = false;
						
						// SLiM constants are reserved too; this code belongs in SLiM, but only
						// SLiM uses this facility right now anyway, so I'm not going to sweat it...
						if (symbol_name == "sim")
							good_symbol = false;
						
						int len = (int)symbol_name.length();
						
						if (len >= 2)
						{
							char first_ch = symbol_name[0];
							
							if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm') || (first_ch == 's'))
							{
								int ch_index;
								
								for (ch_index = 1; ch_index < len; ++ch_index)
								{
									char idx_ch = symbol_name[ch_index];
									
									if ((idx_ch < '0') || (idx_ch > '9'))
										break;
								}
								
								if (ch_index == len)
									good_symbol = false;
							}
						}
						
						// OK, if the symbol name is acceptable, keep digging
						if (good_symbol)
						{
							const EidosASTNode *right_node = top_node->children_[1];
							
							if (right_node)
							{
								auto right_child_count = right_node->children_.size();
								bool is_under_unary_minus = false;
								
								if (right_child_count == 1)
								{
									// if this is a unary minus negating a numeric constant, track that and move down to the operand node
									if (right_node->token_->token_type_ == EidosTokenType::kTokenMinus)
									{
										right_node = right_node->children_[0];
										right_child_count = right_node->children_.size();
										is_under_unary_minus = true;
									}
								}
								
								if (right_child_count == 0)
								{
									EidosValue_SP x_value_sp;
									std::string value_string;
									
									try
									{
										if (right_node->token_->token_type_ == EidosTokenType::kTokenNumber)
										{
											// integer or float; we don't know which, just from tokenizing
											value_string = right_node->token_->token_string_;
											
											if (is_under_unary_minus)
												value_string = "-" + value_string;
											
											x_value_sp = EidosInterpreter::NumericValueForString(value_string, nullptr);
										}
										else if (!is_under_unary_minus)
										{
											if (right_node->token_->token_type_ == EidosTokenType::kTokenString)
											{
												// string
												value_string = right_node->token_->token_string_;
												
												x_value_sp = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(value_string));
											}
											else if (right_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
											{
												// must be either T or F; other identifiers are not legal here
												value_string = right_node->token_->token_string_;
												
												if (value_string == "F")
													x_value_sp = gStaticEidosValue_LogicalF;
												else if (value_string == "T")
													x_value_sp = gStaticEidosValue_LogicalT;
											}
										}
									}
									catch (...)
									{
									}
									
									if (x_value_sp)
									{
										//std::cout << "define " << symbol_name << " = " << value_string << std::endl;
										
										// Permanently alter the global Eidos symbol table; don't do this at home!
										EidosGlobalStringID symbol_id = EidosGlobalStringIDForString(symbol_name);
										EidosSymbolTableEntry table_entry(symbol_id, x_value_sp);
										
										gEidosConstantsSymbolTable->InitializeConstantSymbolEntry(table_entry);
										
										continue;
									}
								}
							}
						}
						else
						{
							gEidosTerminateThrows = save_throws;
							
							EIDOS_TERMINATION << "ERROR (Eidos_DefineConstantsFromCommandLine): illegal defined constant name \"" << symbol_name << "\"." << eidos_terminate(nullptr);
						}
					}
				}
			}
		}
		
		gEidosTerminateThrows = save_throws;
		
		// Terminate without putting out a script line/character diagnostic; that looks weird
		EIDOS_TERMINATION << "ERROR (Eidos_DefineConstantsFromCommandLine): malformed command-line constant definition: " << constant;
		
		if (gEidosTerminateThrows)
		{
			EIDOS_TERMINATION << eidos_terminate(nullptr);
		}
		else
		{
			// This is from operator<<(std::ostream& p_out, const eidos_terminate &p_terminator)
			EIDOS_TERMINATION << std::endl;
			EIDOS_TERMINATION.flush();
			exit(EXIT_FAILURE);
		}
	}
	
	gEidosTerminateThrows = save_throws;
}


// Information on the Context within which Eidos is running (if any).
std::string gEidosContextVersion;
std::string gEidosContextLicense;
std::string gEidosContextCitation;


// the part of the input file that caused an error; used to highlight the token or text that caused the error
int gEidosCharacterStartOfError = -1, gEidosCharacterEndOfError = -1;
int gEidosCharacterStartOfErrorUTF16 = -1, gEidosCharacterEndOfErrorUTF16 = -1;
EidosScript *gEidosCurrentScript = nullptr;
bool gEidosExecutingRuntimeScript = false;

int gEidosErrorLine = -1, gEidosErrorLineCharacter = -1;


// define string stream used for output when gEidosTerminateThrows == 1; otherwise, terminates call exit()
bool gEidosTerminateThrows = true;
std::ostringstream gEidosTermination;
bool gEidosTerminated;


/** Print a demangled stack backtrace of the caller function to FILE* out. */
void eidos_print_stacktrace(FILE *p_out, unsigned int p_max_frames)
{
	fprintf(p_out, "stack trace:\n");
	
	// storage array for stack trace address data
	void* addrlist[p_max_frames+1];
	
	// retrieve current stack addresses
	int addrlen = backtrace(addrlist, static_cast<int>(sizeof(addrlist) / sizeof(void*)));
	
	if (addrlen == 0)
	{
		fprintf(p_out, "  <empty, possibly corrupt>\n");
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
				funcname = ret; // use possibly realloc()-ed string; static analyzer doesn't like this but it is OK I think
				fprintf(p_out, "  %s : %s + %s\n", symbollist[i], funcname, begin_offset);
			}
			else
			{
				// demangling failed. Output function name as a C function with
				// no arguments.
				fprintf(p_out, "  %s : %s() + %s\n", symbollist[i], begin_name, begin_offset);
			}
		}
		else
		{
			// couldn't parse the line? print the whole line.
			fprintf(p_out, "URF:  %s\n", symbollist[i]);
		}
	}
	
	free(funcname);
	free(symbollist);
	
	fflush(p_out);
}

void eidos_script_error_position(int p_start, int p_end, EidosScript *p_script)
{
	gEidosErrorLine = -1;
	gEidosErrorLineCharacter = -1;
	
	if (p_script && (p_start >= 0) && (p_end >= p_start))
	{
		// figure out the script line and position
		const std::string &script_string = p_script->String();
		int length = (int)script_string.length();
		
		if ((length >= p_start) && (length >= p_end))	// == is the EOF position, which we want to allow but have to treat carefully
		{
			int lineStart = (p_start < length) ? p_start : length - 1;
			int lineEnd = (p_end < length) ? p_end : length - 1;
			int lineNumber;
			
			for (; lineStart > 0; --lineStart)
				if ((script_string[lineStart - 1] == '\n') || (script_string[lineStart - 1] == '\r'))
					break;
			for (; lineEnd < length - 1; ++lineEnd)
				if ((script_string[lineEnd + 1] == '\n') || (script_string[lineEnd + 1] == '\r'))
					break;
			
			// Figure out the line number in the script where the error starts
			lineNumber = 1;
			
			for (int i = 0; i < lineStart; ++i)
				if (script_string[i] == '\n')
					lineNumber++;
			
			gEidosErrorLine = lineNumber;
			gEidosErrorLineCharacter = p_start - lineStart;
		}
	}
}

void eidos_log_script_error(std::ostream& p_out, int p_start, int p_end, EidosScript *p_script, bool p_inside_lambda)
{
	if (p_script && (p_start >= 0) && (p_end >= p_start))
	{
		// figure out the script line, print it, show the error position
		const std::string &script_string = p_script->String();
		int length = (int)script_string.length();
		
		if ((length >= p_start) && (length >= p_end))	// == is the EOF position, which we want to allow but have to treat carefully
		{
			int lineStart = (p_start < length) ? p_start : length - 1;
			int lineEnd = (p_end < length) ? p_end : length - 1;
			int lineNumber;
			
			for (; lineStart > 0; --lineStart)
				if ((script_string[lineStart - 1] == '\n') || (script_string[lineStart - 1] == '\r'))
					break;
			for (; lineEnd < length - 1; ++lineEnd)
				if ((script_string[lineEnd + 1] == '\n') || (script_string[lineEnd + 1] == '\r'))
					break;
			
			// Figure out the line number in the script where the error starts
			lineNumber = 1;
			
			for (int i = 0; i < lineStart; ++i)
				if (script_string[i] == '\n')
					lineNumber++;
			
			gEidosErrorLine = lineNumber;
			gEidosErrorLineCharacter = p_start - lineStart;
			
			p_out << std::endl << "Error on script line " << gEidosErrorLine << ", character " << gEidosErrorLineCharacter;
			
			if (p_inside_lambda)
				p_out << " (inside runtime script block)";
			
			p_out << ":" << std::endl << std::endl;
			
			// Emit the script line, converting tabs to three spaces
			for (int i = lineStart; i <= lineEnd; ++i)
			{
				char script_char = script_string[i];
				
				if (script_char == '\t')
					p_out << "   ";
				else if ((script_char == '\n') || (script_char == '\r'))	// don't show more than one line
					break;
				else
					p_out << script_char;
			}
			
			p_out << std::endl;
			
			// Emit the error indicator line, again emitting three spaces where the script had a tab
			for (int i = lineStart; i < p_start; ++i)
			{
				char script_char = script_string[i];
				
				if (script_char == '\t')
					p_out << "   ";
				else if ((script_char == '\n') || (script_char == '\r'))	// don't show more than one line
					break;
				else
					p_out << ' ';
			}
			
			// Emit the error indicator
			for (int i = 0; i < p_end - p_start + 1; ++i)
				p_out << "^";
			
			p_out << std::endl;
		}
	}
}

eidos_terminate::eidos_terminate(const EidosToken *p_error_token)
{
	// This is the end of the line, so we don't need to treat the error position as a stack
	if (p_error_token)
		EidosScript::PushErrorPositionFromToken(p_error_token);
}

eidos_terminate::eidos_terminate(bool p_print_backtrace) : print_backtrace_(p_print_backtrace)
{
}

eidos_terminate::eidos_terminate(const EidosToken *p_error_token, bool p_print_backtrace) : print_backtrace_(p_print_backtrace)
{
	// This is the end of the line, so we don't need to treat the error position as a stack
	if (p_error_token)
		EidosScript::PushErrorPositionFromToken(p_error_token);
}

void operator<<(std::ostream& p_out, const eidos_terminate &p_terminator)
{
	p_out << std::endl;
	
	p_out.flush();
	
	if (p_terminator.print_backtrace_)
		eidos_print_stacktrace(stderr);
	
	if (gEidosTerminateThrows)
	{
		// In this case, eidos_terminate() throws an exception that gets caught by the Context.  That invalidates the simulation object, and
		// causes the Context to display an error message and ends the simulation run, but it does not terminate the app.
		throw std::runtime_error("A runtime error occurred in Eidos");
	}
	else
	{
		// In this case, eidos_terminate() does in fact terminate; this is appropriate when errors are simply fatal and there is no UI.
		// In this case, we want to emit a diagnostic showing the line of script where the error occurred, if we can.
		// This facility uses only the non-UTF16 positions, since it is based on std::string, so those positions can be ignored.
		eidos_log_script_error(p_out, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
		
		exit(EXIT_FAILURE);
	}
}

std::string EidosGetTrimmedRaiseMessage(void)
{
	if (gEidosTerminateThrows)
	{
		std::string terminationMessage = gEidosTermination.str();
		
		gEidosTermination.clear();
		gEidosTermination.str(gEidosStr_empty_string);
		
		// trim off newlines at the end of the raise string
		size_t endpos = terminationMessage.find_last_not_of("\n\r");
		if (std::string::npos != endpos)
			terminationMessage = terminationMessage.substr(0, endpos + 1);
		
		return terminationMessage;
	}
	else
	{
		return gEidosStr_empty_string;
	}
}

std::string EidosGetUntrimmedRaiseMessage(void)
{
	if (gEidosTerminateThrows)
	{
		std::string terminationMessage = gEidosTermination.str();
		
		gEidosTermination.clear();
		gEidosTermination.str(gEidosStr_empty_string);
		
		return terminationMessage;
	}
	else
	{
		return gEidosStr_empty_string;
	}
}


//
//	The code below was obtained from http://nadeausoftware.com/articles/2012/07/c_c_tip_how_get_process_resident_set_size_physical_memory_use.  It may or may not work.
//	On Windows, it requires linking with Microsoft's psapi.lib.  That is left as an exercise for the reader.  Nadeau says "On other OSes, the default libraries are sufficient."
//

/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#include <stdio.h>

#endif

#else
#error "Cannot define getPeakRSS( ) or getCurrentRSS( ) for an unknown OS."
#endif

/**
 * Returns the peak (maximum so far) resident set size (physical
 * memory use) measured in bytes, or zero if the value cannot be
 * determined on this OS.
 */
size_t EidosGetPeakRSS(void)
{
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
	return (size_t)info.PeakWorkingSetSize;
	
#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
	/* AIX and Solaris ------------------------------------------ */
	struct psinfo psinfo;
	int fd = -1;
	if ( (fd = open( "/proc/self/psinfo", O_RDONLY )) == -1 )
		return (size_t)0L;		/* Can't open? */
	if ( read( fd, &psinfo, sizeof(psinfo) ) != sizeof(psinfo) )
	{
		close( fd );
		return (size_t)0L;		/* Can't read? */
	}
	close( fd );
	return (size_t)(psinfo.pr_rssize * 1024L);
	
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	/* BSD, Linux, and OSX -------------------------------------- */
	struct rusage rusage;
	getrusage( RUSAGE_SELF, &rusage );
#if defined(__APPLE__) && defined(__MACH__)
	return (size_t)rusage.ru_maxrss;
#else
	return (size_t)(rusage.ru_maxrss * 1024L);
#endif
	
#else
	/* Unknown OS ----------------------------------------------- */
	return (size_t)0L;			/* Unsupported. */
#endif
}


/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
size_t EidosGetCurrentRSS(void)
{
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
	return (size_t)info.WorkingSetSize;
	
#elif defined(__APPLE__) && defined(__MACH__)
	/* OSX ------------------------------------------------------ */
	struct mach_task_basic_info info;
	mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
	if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,
				   (task_info_t)&info, &infoCount ) != KERN_SUCCESS )
		return (size_t)0L;		/* Can't access? */
	return (size_t)info.resident_size;
	
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
	/* Linux ---------------------------------------------------- */
	long rss = 0L;
	FILE* fp = NULL;
	if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
		return (size_t)0L;		/* Can't open? */
	if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
	{
		fclose( fp );
		return (size_t)0L;		/* Can't read? */
	}
	fclose( fp );
	return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);
	
#else
	/* AIX, BSD, Solaris, and Unknown OS ------------------------ */
	return (size_t)0L;			/* Unsupported. */
#endif
}


// resolve a leading ~ in a filesystem path to the user's home directory
std::string EidosResolvedPath(const std::string p_path)
{
	std::string path = p_path;
	
	// if there is a leading '~', replace it with the user's home directory; not sure if this works on Windows...
	if ((path.length() > 0) && (path[0] == '~'))
	{
		const char *homedir;
		
		if ((homedir = getenv("HOME")) == NULL)
			homedir = getpwuid(getuid())->pw_dir;
		
		if (strlen(homedir))
			path.replace(0, 1, homedir);
	}
	
	return path;
}

// run a Un*x command; thanks to http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
std::string EidosExec(const char* cmd)
{
	char buffer[128];
	std::string result = "";
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe) throw std::runtime_error("popen() failed!");
	while (!feof(pipe.get())) {
		if (fgets(buffer, 128, pipe.get()) != NULL)
			result += buffer;
	}
	return result;
}

size_t EidosGetMaxRSS(void)
{
	static bool beenHere = false;
	static size_t max_rss = 0;
	
	if (!beenHere)
	{
#if 0
		// Find our RSS limit by launching a subshell to run "ulimit -m"
		std::string limit_string = EidosExec("ulimit -m");
		
		std::string unlimited("unlimited");
		
		if (std::mismatch(unlimited.begin(), unlimited.end(), limit_string.begin()).first == unlimited.end())
		{
			// "unlimited" is a prefix of foobar, so use 0 to represent that
			max_rss = 0;
		}
		else
		{
			errno = 0;
			
			const char *c_str = limit_string.c_str();
			char *last_used_char = nullptr;
			
			max_rss = strtoq(c_str, &last_used_char, 10);
			
			if (errno || (last_used_char == c_str))
			{
				// If an error occurs, assume we are unlimited
				max_rss = 0;
			}
			else
			{
				// This value is in 1024-byte units, so multiply to get a limit in bytes
				max_rss *= 1024L;
			}
		}
#else
		// Find our RSS limit using getrlimit() – easier and safer
		struct rlimit rlim;
		
		if (getrlimit(RLIMIT_RSS, &rlim) == 0)
		{
			// This value is in bytes, no scaling needed
			max_rss = (uint64_t)rlim.rlim_max;
			
			// If the claim is that we have more than 1024 TB at our disposal, then we will consider ourselves unlimited :->
			if (max_rss > 1024L * 1024L * 1024L * 1024L * 1024L)
				max_rss = 0;
		}
		else
		{
			// If an error occurs, assume we are unlimited
			max_rss = 0;
		}
#endif
		
		beenHere = true;
	}
	
	return max_rss;
}

void EidosCheckRSSAgainstMax(std::string p_message1, std::string p_message2)
{
	static bool beenHere = false;
	static size_t max_rss = 0;
	
	if (!beenHere)
	{
		// The first time we are called, we get the memory limit and sanity-check it
		max_rss = EidosGetMaxRSS();
		
#if 0
		// Impose a 20 MB limit, for testing
		max_rss = 20*1024*1024;
#warning Turn this off!
#endif
		
		if (max_rss != 0)
		{
			size_t current_rss = EidosGetCurrentRSS();
			
			// If we are already within 10 MB of overrunning our supposed limit, disable checking; assume that
			// either EidosGetMaxRSS() or EidosGetCurrentRSS() is not telling us the truth.
			if (current_rss + 10L*1024L*1024L > max_rss)
				max_rss = 0;
		}
		
		// Switch off our memory check flag if we are not going to enforce a limit anyway;
		// this allows the caller to skip calling us when possible, for speed
		if (max_rss == 0)
			eidos_do_memory_checks = false;
		
		beenHere = true;
	}
	
	if (eidos_do_memory_checks && (max_rss != 0))
	{
		size_t current_rss = EidosGetCurrentRSS();
		
		// If we are within 10 MB of overrunning our limit, then terminate with a message before
		// the system does it for us.  10 MB gives us a little headroom, so that we detect this
		// condition before the system does.
		if (current_rss + 10L*1024L*1024L > max_rss)
		{
			// We output our warning to std::cerr, because we may get killed by the OS for exceeding our memory limit before other streams would get flushed
			std::cerr << "WARNING (" << p_message1 << "): memory usage of " << (current_rss / (1024.0 * 1024.0)) << " MB is dangerously close to the limit of " << (max_rss / (1024.0 * 1024.0)) << " MB reported by the operating system.  This SLiM process may soon be killed by the operating system for exceeding the memory limit.  You might raise the per-process memory limit, or modify your model to decrease memory usage.  You can turn off this memory check with the '-x' command-line option.  " << p_message2 << std::endl;
			std::cerr.flush();
			
			// We want to issue only one warning, so turn off warnings now
			eidos_do_memory_checks = false;
		}
	}
}


//	Global std::string objects.
const std::string gEidosStr_empty_string = "";
const std::string gEidosStr_space_string = " ";

// mostly function names used in multiple places
const std::string gEidosStr_function = "function";
const std::string gEidosStr_method = "method";
const std::string gEidosStr_apply = "apply";
const std::string gEidosStr_doCall = "doCall";
const std::string gEidosStr_executeLambda = "executeLambda";
const std::string gEidosStr_ls = "ls";
const std::string gEidosStr_rm = "rm";

// mostly language keywords
const std::string gEidosStr_if = "if";
const std::string gEidosStr_else = "else";
const std::string gEidosStr_do = "do";
const std::string gEidosStr_while = "while";
const std::string gEidosStr_for = "for";
const std::string gEidosStr_in = "in";
const std::string gEidosStr_next = "next";
const std::string gEidosStr_break = "break";
const std::string gEidosStr_return = "return";

// mostly Eidos global constants
const std::string gEidosStr_T = "T";
const std::string gEidosStr_F = "F";
const std::string gEidosStr_NULL = "NULL";
const std::string gEidosStr_PI = "PI";
const std::string gEidosStr_E = "E";
const std::string gEidosStr_INF = "INF";
const std::string gEidosStr_MINUS_INF = "-INF";
const std::string gEidosStr_NAN = "NAN";

// mostly Eidos type names
const std::string gEidosStr_void = "void";
const std::string gEidosStr_logical = "logical";
const std::string gEidosStr_string = "string";
const std::string gEidosStr_integer = "integer";
const std::string gEidosStr_float = "float";
const std::string gEidosStr_object = "object";
const std::string gEidosStr_numeric = "numeric";

// Eidos function names, property names, and method names
const std::string gEidosStr_size = "size";
const std::string gEidosStr_property = "property";
const std::string gEidosStr_str = "str";

// other miscellaneous strings
const std::string gEidosStr_GetPropertyOfElements = "GetPropertyOfElements";
const std::string gEidosStr_ExecuteInstanceMethod = "ExecuteInstanceMethod";
const std::string gEidosStr_undefined = "undefined";
const std::string gEidosStr_applyValue = "applyValue";

// strings for Eidos_TestElement
const std::string gEidosStr__TestElement = "_TestElement";
const std::string gEidosStr__yolk = "_yolk";
const std::string gEidosStr__increment = "_increment";
const std::string gEidosStr__cubicYolk = "_cubicYolk";
const std::string gEidosStr__squareTest = "_squareTest";

// strings for parameters, function names, etc., that are needed as explicit registrations in a Context and thus have to be
// explicitly registered by Eidos; see the comment in Eidos_RegisterStringForGlobalID() below
const std::string gEidosStr_weights = "weights";
const std::string gEidosStr_n = "n";


static std::unordered_map<std::string, EidosGlobalStringID> gStringToID;
static std::unordered_map<EidosGlobalStringID, const std::string *> gIDToString;
static EidosGlobalStringID gNextUnusedID = gEidosID_LastContextEntry;


void Eidos_RegisterStringForGlobalID(const std::string &p_string, EidosGlobalStringID p_string_id)
{
	// BCH 13 September 2016: So, this is a tricky issue without a good resolution at the moment.  Eidos explicitly registers
	// a few strings, using this method, right below in Eidos_RegisterGlobalStringsAndIDs().  And SLiM explicitly registers
	// a bunch more strings, in SLiM_RegisterGlobalStringsAndIDs().  So far so good.  But Eidos also registers a bunch of
	// strings "in passing", as a side effect of calling EidosGlobalStringIDForString(), because it doesn't care what the
	// IDs of those strings are, it just wants them to be registered for later matching.  This happens to function names and
	// parameter names, in particular.  This is good; we don't want to have to explicitly enumerate and register all of those
	// strings, that would be a tremendous pain.  The problem is that these "in passing" registrations can conflict with
	// registrations done in the Context, unpredictably.  A new parameter named "weights" is added to a new Eidos function,
	// and suddenly the explicit registration of "weights" in SLiM has broken and needs to be removed.  At least you know
	// that that has, happened, because you end up here.  When you end up here, don't just comment out the registration call
	// in the Context; you also need to add an explicit registration call in Eidos, and remove the string and ID definitions
	// in the Context, and so forth.  Migrate the whole explicit registration from the Context back into Eidos.  Unfortunate,
	// but I don't see any good solution.  Sure is nice how uniquing of selectors just happens automatically in Obj-C!  That
	// is basically what we're trying to duplicate here, without language support.
	if (gStringToID.find(p_string) != gStringToID.end())
		EIDOS_TERMINATION << "ERROR (Eidos_RegisterStringForGlobalID): string " << p_string << " has already been registered." << eidos_terminate(nullptr);
	
	if (gIDToString.find(p_string_id) != gIDToString.end())
		EIDOS_TERMINATION << "ERROR (Eidos_RegisterStringForGlobalID): id " << p_string_id << " has already been registered." << eidos_terminate(nullptr);
	
	if (p_string_id >= gEidosID_LastContextEntry)
		EIDOS_TERMINATION << "ERROR (Eidos_RegisterStringForGlobalID): id " << p_string_id << " it out of the legal range for preregistered strings." << eidos_terminate(nullptr);
	
	gStringToID[p_string] = p_string_id;
	gIDToString[p_string_id] = &p_string;
}

void Eidos_RegisterGlobalStringsAndIDs(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		Eidos_RegisterStringForGlobalID(gEidosStr_method, gEidosID_method);
		Eidos_RegisterStringForGlobalID(gEidosStr_size, gEidosID_size);
		Eidos_RegisterStringForGlobalID(gEidosStr_property, gEidosID_property);
		Eidos_RegisterStringForGlobalID(gEidosStr_str, gEidosID_str);
		Eidos_RegisterStringForGlobalID(gEidosStr_applyValue, gEidosID_applyValue);
		
		Eidos_RegisterStringForGlobalID(gEidosStr_T, gEidosID_T);
		Eidos_RegisterStringForGlobalID(gEidosStr_F, gEidosID_F);
		Eidos_RegisterStringForGlobalID(gEidosStr_NULL, gEidosID_NULL);
		Eidos_RegisterStringForGlobalID(gEidosStr_PI, gEidosID_PI);
		Eidos_RegisterStringForGlobalID(gEidosStr_E, gEidosID_E);
		Eidos_RegisterStringForGlobalID(gEidosStr_INF, gEidosID_INF);
		Eidos_RegisterStringForGlobalID(gEidosStr_NAN, gEidosID_NAN);
		
		Eidos_RegisterStringForGlobalID(gEidosStr__TestElement, gEidosID__TestElement);
		Eidos_RegisterStringForGlobalID(gEidosStr__yolk, gEidosID__yolk);
		Eidos_RegisterStringForGlobalID(gEidosStr__increment, gEidosID__increment);
		Eidos_RegisterStringForGlobalID(gEidosStr__cubicYolk, gEidosID__cubicYolk);
		Eidos_RegisterStringForGlobalID(gEidosStr__squareTest, gEidosID__squareTest);
		
		Eidos_RegisterStringForGlobalID(gEidosStr_weights, gEidosID_weights);
		Eidos_RegisterStringForGlobalID(gEidosStr_n, gEidosID_n);
	}
}

EidosGlobalStringID EidosGlobalStringIDForString(const std::string &p_string)
{
	//std::cerr << "EidosGlobalStringIDForString: " << p_string << std::endl;
	auto found_iter = gStringToID.find(p_string);
	
	if (found_iter == gStringToID.end())
	{
		// If the hash table does not already contain this key, then we add it to the table as a side effect.
		// We have to copy the string, because we have no idea what the caller might do with the string they passed us.
		// Since the hash table makes its own copy of the key, this copy is used only for the value in the hash tables.
		const std::string *copied_string = new const std::string(p_string);
		uint32_t string_id = gNextUnusedID++;
		
		gStringToID[*copied_string] = string_id;
		gIDToString[string_id] = copied_string;
		
		return string_id;
	}
	else
		return found_iter->second;
}

const std::string &StringForEidosGlobalStringID(EidosGlobalStringID p_string_id)
{
	//std::cerr << "StringForEidosGlobalStringID: " << p_string_id << std::endl;
	auto found_iter = gIDToString.find(p_string_id);
	
	if (found_iter == gIDToString.end())
		return gEidosStr_undefined;
	else
		return *(found_iter->second);
}


















































