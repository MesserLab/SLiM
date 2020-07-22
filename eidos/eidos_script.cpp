//
//  eidos_script.cpp
//  Eidos
//
//  Created by Ben Haller on 4/1/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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


#include "eidos_script.h"
#include "eidos_globals.h"
#include "eidos_value.h"
#include "eidos_interpreter.h"
#include "eidos_ast_node.h"

#include <iostream>
#include <sstream>


// set these to true to get logging of tokens / AST / evaluation
bool gEidosLogTokens = false;
bool gEidosLogAST = false;
bool gEidosLogEvaluation = false;


// From stack overflow http://stackoverflow.com/questions/15038616/how-to-convert-between-character-and-byte-position-in-objective-c-c-c
// by Dietrich Epp.  This function converts a "character" position in, e.g., std::string, which uses UTF-8, to a "character"
// position in, e.g., NSString, which uses UTF-16.  We don't actually use Eidos_utf8_utf16width(), but we use BYTE_WIDTHS,
// so I've kept Eidos_utf8_utf16width() here as a sort of documentation for what BYTE_WIDTHS means.
static const unsigned char BYTE_WIDTHS[256] = {
	// 1-byte: 0xxxxxxx
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	// Trailing: 10xxxxxx
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	// 2-byte leading: 110xxxxx
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	// 3-byte leading: 1110xxxx
	// 4-byte leading: 11110xxx
	// invalid: 11111xxx
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0
};

/*
static size_t Eidos_utf8_utf16width(const unsigned char *string, size_t len)
{
	size_t i, utf16len = 0;
	for (i = 0; i < len; i++)
		utf16len += BYTE_WIDTHS[string[i]];
	return utf16len;
}
*/


//
//	Script
//
#pragma mark -
#pragma mark EidosScript
#pragma mark -

EidosScript::EidosScript(const std::string &p_script_string) :
	script_string_(p_script_string)
{
}

EidosScript::~EidosScript(void)
{
	// destroy the parse root and return it to the pool; the tree must be allocated out of gEidosASTNodePool!
	if (parse_root_)
	{
		parse_root_->~EidosASTNode();
		gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(parse_root_));
		parse_root_ = nullptr;
	}
}

void EidosScript::Tokenize(bool p_make_bad_tokens, bool p_keep_nonsignificant)
{
	// set up error tracking for this script
	// Note: Here and elsewhere in this method, if p_make_bad_tokens is set we do not do error tracking.  This
	// is so that we don't overwrite valid error tracking info when we're tokenizing for internal purposes.
	EidosScript *current_script_save = gEidosCurrentScript;
	if (!p_make_bad_tokens)
		gEidosCurrentScript = this;
	
	// delete all existing tokens
	token_stream_.clear();
	
	// destroy the parse root and return it to the pool; the tree must be allocated out of gEidosASTNodePool!
	if (parse_root_)
	{
		parse_root_->~EidosASTNode();
		gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(parse_root_));
		parse_root_ = nullptr;
	}
	
	// chew off one token at a time from script_string_, make a Token object, and add it
	int32_t pos = 0, len = (int)script_string_.length();
	int32_t pos_UTF16 = 0;
	
	while (pos < len)
	{
		int32_t token_start = pos;													// the first character position in the current token
		int32_t token_end = pos;													// the last character position in the current token
		int ch = (unsigned char)script_string_[pos];								// the current character
		int ch2 = ((pos >= len) ? 0 : (unsigned char)script_string_[pos + 1]);		// look ahead one character (assuming ch is a single-byte character)
		bool skip = false;															// set to true to skip creating/adding this token
		EidosTokenType token_type = EidosTokenType::kTokenNone;
		std::string token_string;
		int32_t token_UTF16_start = pos_UTF16;
		int32_t token_UTF16_end = pos_UTF16;
		
		switch (ch)
		{
			// cases that require just a single character to match
			case ';': token_type = EidosTokenType::kTokenSemicolon; break;
			case ':': token_type = EidosTokenType::kTokenColon; break;
			case ',': token_type = EidosTokenType::kTokenComma; break;
			case '{': token_type = EidosTokenType::kTokenLBrace; break;
			case '}': token_type = EidosTokenType::kTokenRBrace; break;
			case '(': token_type = EidosTokenType::kTokenLParen; break;
			case ')': token_type = EidosTokenType::kTokenRParen; break;
			case '[': token_type = EidosTokenType::kTokenLBracket; break;
			case ']': token_type = EidosTokenType::kTokenRBracket; break;
			case '.': token_type = EidosTokenType::kTokenDot; break;
			case '+': token_type = EidosTokenType::kTokenPlus; break;
			case '-': token_type = EidosTokenType::kTokenMinus; break;
			case '*': token_type = EidosTokenType::kTokenMult; break;
			case '^': token_type = EidosTokenType::kTokenExp; break;
			case '%': token_type = EidosTokenType::kTokenMod; break;
			case '&': token_type = EidosTokenType::kTokenAnd; break;
			case '|': token_type = EidosTokenType::kTokenOr; break;
			case '?': token_type = EidosTokenType::kTokenConditional; break;
			case '$': token_type = EidosTokenType::kTokenSingleton; break;
			
			// cases that require lookahead due to ambiguity: =, <, >, !, /
			case '=':
				if (ch2 == '=') { token_type = EidosTokenType::kTokenEq; token_end++; token_UTF16_end++; }
				else { token_type = EidosTokenType::kTokenAssign; }
				break;
			case '<':	// <<DELIM "here document"-style string, or <= or <
				if (ch2 == '<')
				{
					// A "here document" string literal; starts with <<DELIM and ends with >>DELIM, where DELIM is any
					// character sequence (including no delim) followed by a newline that is not part of DELIM.
					// This style of string literal has no escape sequences, and allows newlines.
					token_type = EidosTokenType::kTokenString;
					
					// find the delimiter; it may be any characters at all, or none, followed by a newline or EOF
					// there is always at least a zero-length delimiter, so this cannot fail
					int32_t delim_start_pos = pos + 2;
					int32_t delim_end_pos = pos + 1;
					int32_t delim_end_pos_UTF16 = pos_UTF16 + 1;
					int32_t delim_length_UTF16 = 0;
					
					while (delim_end_pos + 1 < len)
					{
						unsigned char chn = (unsigned char)script_string_[delim_end_pos + 1];
						
						if ((chn == '\n') || (chn == '\r'))
							break;
						
						delim_end_pos++;
						delim_end_pos_UTF16 += BYTE_WIDTHS[chn];
						delim_length_UTF16 += BYTE_WIDTHS[chn];
					}
					
					// now move characters into the string literal until we find a newline followed by the end-delimiter
					int delim_length = delim_end_pos - delim_start_pos + 1;
					
					token_end = delim_end_pos + 1;	// skip the initial newline, which is not part of the string literal
					token_UTF16_end = delim_end_pos_UTF16 + 1;
					
					while (true)
					{
						if (token_end + 1 >= len)
						{
							if (p_make_bad_tokens)
							{
								token_type = EidosTokenType::kTokenBad;
								break;
							}
							
							gEidosCharacterStartOfError = token_start;
							gEidosCharacterEndOfError = token_end;
							gEidosCharacterStartOfErrorUTF16 = token_UTF16_start;
							gEidosCharacterEndOfErrorUTF16 = token_UTF16_end;
							
							EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): unexpected EOF in custom-delimited string literal." << EidosTerminate();
						}
						
						unsigned char chn = (unsigned char)script_string_[token_end + 1];
						
						if (((chn == '\n') || (chn == '\r')) && (token_end + 1 + delim_length + 2 < len))	// +1 for the newlines, +2 for ">>", plus the delimiter itself, must all fit before the EOF
						{
							if ((script_string_[token_end + 2] == '>') && (script_string_[token_end + 3] == '>'))
							{
								int delim_index;
								
								for (delim_index = 0; delim_index < delim_length; ++delim_index)
									if (script_string_[token_end + 4 + delim_index] != script_string_[delim_start_pos + delim_index])
										break;
								
								if (delim_index == delim_length)
								{
									// the full delimiter matched, so we are done; advance by newline + '>' + '>' + delimiter
									token_end = token_end + 3 + delim_length;
									token_UTF16_end = token_UTF16_end + 3 + delim_length_UTF16;
									break;
								}
							}
						}
						
						// the current character is not the start of an end-delimiter, so it is part of the token's string
						token_string += chn;
						token_end++;
						token_UTF16_end += BYTE_WIDTHS[chn];
					}
				}
				else
				{
					if (ch2 == '=') { token_type = EidosTokenType::kTokenLtEq; token_end++; token_UTF16_end++; }
					else { token_type = EidosTokenType::kTokenLt; }
				}
				break;
			case '>':	// >= or >
				if (ch2 == '=') { token_type = EidosTokenType::kTokenGtEq; token_end++; token_UTF16_end++; }
				else { token_type = EidosTokenType::kTokenGt; }
				break;
			case '!':	// != or !
				if (ch2 == '=') { token_type = EidosTokenType::kTokenNotEq; token_end++; token_UTF16_end++; }
				else { token_type = EidosTokenType::kTokenNot; }
				break;
			case '/':	// // or /* or /
				if (ch2 == '/') {
					token_type = EidosTokenType::kTokenComment;
					skip = true;
					
					// stop at the end of the input string, unless we see a newline first
					while (token_end + 1 < len)
					{
						unsigned char chn = (unsigned char)script_string_[token_end + 1];
						
						// stop short of eating the newline
						if ((chn == '\n') || (chn == '\r'))
							break;
						
						token_end++;
						token_UTF16_end += BYTE_WIDTHS[chn];
					}
				}
				else if (ch2 == '*') {
					token_type = EidosTokenType::kTokenCommentLong;
					skip = true;
					
					// eat the asterisk
					token_end++;
					token_UTF16_end += BYTE_WIDTHS[ch2];
					
					int nest_count = 1;		// /* */ comments in Eidos nest properly, so we keep a count
					
					// stop at the end of the input string, unless we see a terminator first
					while (token_end + 1 < len)
					{
						unsigned char chn = (unsigned char)script_string_[token_end + 1];
						
						if (chn == '*')
						{
							if (token_end + 2 < len)
							{
								unsigned char chnn = (unsigned char)script_string_[token_end + 2];
								
								if (chnn == '/')
								{
									// We see a */, so eat it
									token_end++;
									token_UTF16_end += BYTE_WIDTHS[chn];
									
									token_end++;
									token_UTF16_end += BYTE_WIDTHS[chnn];
									
									if (--nest_count == 0)
										break;
									continue;
								}
							}
						}
						else if (chn == '/')
						{
							if (token_end + 2 < len)
							{
								unsigned char chnn = (unsigned char)script_string_[token_end + 2];
								
								if (chnn == '*')
								{
									// We see a /*, so eat it
									token_end++;
									token_UTF16_end += BYTE_WIDTHS[chn];
									
									token_end++;
									token_UTF16_end += BYTE_WIDTHS[chnn];
									
									++nest_count;
									continue;
								}
							}
						}
						
						token_end++;
						token_UTF16_end += BYTE_WIDTHS[chn];
					}
				}
				else { token_type = EidosTokenType::kTokenDiv; }
				break;
			
			// cases that require advancement: numbers, strings, identifiers, identifier-like tokens, whitespace
			default:
				if ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'))
				{
					// whitespace; any nonzero-length sequence of space, tab, \n, \r
					// FIXME it would be nice for &nbsp; to be considered whitespace too, but that gets bogged down
					// in encoding issues I guess; we are not very Unicode-friendly right now.  BCH 2 Nov. 2017
					while (token_end + 1 < len)
					{
						int chn = (unsigned char)script_string_[token_end + 1];
						
						if ((chn == ' ') || (chn == '\t') || (chn == '\n') || (chn == '\r'))
						{
							token_end++;
							token_UTF16_end++;
						}
						else
							break;
					}
					
					token_type = EidosTokenType::kTokenWhitespace;
					skip = true;
				}
				else if ((ch >= '0') && (ch <= '9'))
				{
					// number: regex something like this, off the top of my head: [0-9]+(\.[0-9]*)?([e|E][-|+]?[0-9]+)?
					while (token_end + 1 < len)
					{
						int chn = (unsigned char)script_string_[token_end + 1];
						
						if ((chn >= '0') && (chn <= '9'))
						{
							token_end++;
							token_UTF16_end++;
						}
						else
							break;
					}
					
					// optional decimal sequence
					if ((token_end + 1 < len) && (script_string_[token_end + 1] == '.'))
					{
						token_end++;
						token_UTF16_end++;
						
						while (token_end + 1 < len)
						{
							int chn = (unsigned char)script_string_[token_end + 1];
							
							if ((chn >= '0') && (chn <= '9'))
							{
								token_end++;
								token_UTF16_end++;
							}
							else
								break;
						}
					}
					
					// optional exponent sequence
					if ((token_end + 1 < len) && ((script_string_[token_end + 1] == 'e') || (script_string_[token_end + 1] == 'E')))
					{
						token_end++;
						token_UTF16_end++;
						
						// optional sign
						if ((token_end + 1 < len) && ((script_string_[token_end + 1] == '+') || (script_string_[token_end + 1] == '-')))
						{
							token_end++;
							token_UTF16_end++;
						}
						
						// mandatory exponent value; if this is missing, we drop through and token_type == EidosTokenType::kTokenNone
						if ((token_end + 1 < len) && ((script_string_[token_end + 1] >= '0') && (script_string_[token_end + 1] <= '9')))
						{
							while (token_end + 1 < len)
							{
								int chn = (unsigned char)script_string_[token_end + 1];
								
								if ((chn >= '0') && (chn <= '9'))
								{
									token_end++;
									token_UTF16_end++;
								}
								else
									break;
							}
							
							token_type = EidosTokenType::kTokenNumber;
						}
					}
					else
					{
						token_type = EidosTokenType::kTokenNumber;
					}
				}
				else if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || (ch == '_'))
				{
					// identifier: regex something like this: [a-zA-Z_][a-zA-Z0-9_]*
					while (token_end + 1 < len)
					{
						int chn = (unsigned char)script_string_[token_end + 1];
						
						if (((chn >= 'a') && (chn <= 'z')) || ((chn >= 'A') && (chn <= 'Z')) || ((chn >= '0') && (chn <= '9')) || (chn == '_'))
						{
							token_end++;
							token_UTF16_end++;
						}
						else
							break;
					}
					
					token_type = EidosTokenType::kTokenIdentifier;
				}
				else if ((ch == '"') || (ch == '\''))
				{
					bool double_quoted = (ch == '"');
					
					// string literal: bounded by double quotes, with escapes (\t, \r, \n, \", \', \\), newlines not allowed
					token_type = EidosTokenType::kTokenString;
					
					do 
					{
						// unlike most other tokens, string literals do not terminate automatically at EOF or an illegal character
						if (token_end + 1 == len)
						{
							if (p_make_bad_tokens)
							{
								token_type = EidosTokenType::kTokenBad;
								break;
							}
							
							gEidosCharacterStartOfError = token_start;
							gEidosCharacterEndOfError = token_end;
							gEidosCharacterStartOfErrorUTF16 = token_UTF16_start;
							gEidosCharacterEndOfErrorUTF16 = token_UTF16_end;
							
							EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): unexpected EOF in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << EidosTerminate();
						}
						
						unsigned char chn = (unsigned char)script_string_[token_end + 1];
						
						if (chn == (double_quoted ? '"' : '\''))
						{
							// end of string
							token_end++;
							token_UTF16_end++;
							break;
						}
						else if (chn == '\\')
						{
							// escape sequence; another character must exist
							if (token_end + 2 == len)
							{
								if (p_make_bad_tokens)
								{
									token_type = EidosTokenType::kTokenBad;
									break;
								}
								
								gEidosCharacterStartOfError = token_start;
								gEidosCharacterEndOfError = token_end;
								gEidosCharacterStartOfErrorUTF16 = token_UTF16_start;
								gEidosCharacterEndOfErrorUTF16 = token_UTF16_end;
								
								EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): unexpected EOF in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << EidosTerminate();
							}
							
							unsigned char ch_esq = (unsigned char)script_string_[token_end + 2];
							
							if ((ch_esq == 't') || (ch_esq == 'r') || (ch_esq == 'n') || (ch_esq == '"') || (ch_esq == '\'') || (ch_esq == '\\'))
							{
								// a legal escape, so replace it with the escaped character here
								if (ch_esq == 't')			token_string += '\t';
								else if (ch_esq == 'r')		token_string += '\r';
								else if (ch_esq == 'n')		token_string += '\n';
								else if (ch_esq == '"')		token_string += '"';
								else if (ch_esq == '\'')	token_string += '\'';
								else if (ch_esq == '\\')	token_string += '\\';
								
								token_end += 2;
								token_UTF16_end += 2;
							}
							else
							{
								// an illegal escape; if we are making bad tokens, we tolerate the error and continue
								if (p_make_bad_tokens)
								{
									token_string += ch_esq;
									token_end += 2;
									token_UTF16_end += 1 + BYTE_WIDTHS[ch_esq];
								}
								else
								{
									gEidosCharacterStartOfError = token_end + 1;
									gEidosCharacterEndOfError = token_end + 2;
									gEidosCharacterStartOfErrorUTF16 = token_UTF16_end + 1;
									gEidosCharacterEndOfErrorUTF16 = token_UTF16_end + 1 + BYTE_WIDTHS[ch_esq];
									
									EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): illegal escape \\" << (char)ch_esq << " in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << EidosTerminate();
								}
							}
						}
						else if ((chn == '\n') || (chn == '\r'))
						{
							// literal newlines are not allowed within string literals at present
							if (p_make_bad_tokens)
							{
								token_type = EidosTokenType::kTokenBad;
								break;
							}
							
							gEidosCharacterStartOfError = token_start;
							gEidosCharacterEndOfError = token_end;
							gEidosCharacterStartOfErrorUTF16 = token_UTF16_start;
							gEidosCharacterEndOfErrorUTF16 = token_UTF16_end;
							
							EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): illegal newline in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << EidosTerminate();
						}
						else
						{
							// all other characters get eaten up as part of the string literal
							token_string += chn;
							token_end++;
							token_UTF16_end += BYTE_WIDTHS[chn];
						}
					}
					while (true);
				}
				else if (ch & 0x0080)
				{
					// The high bit is set, so this is some sort of Unicode special byte, initiating a multi-byte sequence comprising an
					// illegal non-ASCII character; encompass the whole thing into the token so errors, bad tokens, etc. work correctly
					token_end++;
					token_UTF16_end += BYTE_WIDTHS[ch];
					
					while (token_end < len)
					{
						int chn = (unsigned char)script_string_[token_end];
						
						if ((chn & 0x00C0) == 0x00C0)
						{
							// the two high bits are both set, so this is the beginning of a successive Unicode multi-byte sequence, which we don't want to run into
							break;
						}
						else if (chn & 0x0080)
						{
							// the high bit is set, so this is a trailing byte of the current Unicode multi-byte sequence, so eat it
							token_end++;
							token_UTF16_end += BYTE_WIDTHS[chn];
						}
						else
						{
							// the high bit is not set, so this is an ordinary character following the Unicode sequence, which we don't want to run into
							break;
						}
					}
					
					token_end--;
					token_UTF16_end--;
				}
				// else: ch is an ASCII-range single character that does not match any possible token, so it will be handled by the token_type == EidosTokenType::kTokenNone case directly below
				break;
		}
		
		if (token_type == EidosTokenType::kTokenNone)
		{
			// failed to find a match; this causes either a syntax error raise or a bad token
			if (p_make_bad_tokens)
			{
				token_type = EidosTokenType::kTokenBad;
			}
			else
			{
				// during tokenization we don't treat the error position as a stack
				gEidosCharacterStartOfError = token_start;
				gEidosCharacterEndOfError = token_end;
				gEidosCharacterStartOfErrorUTF16 = token_UTF16_start;
				gEidosCharacterEndOfErrorUTF16 = token_UTF16_end;
				
				EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): unrecognized token at '" << script_string_.substr(token_start, token_end - token_start + 1) << "'." << EidosTerminate();
			}
		}
		
		// if skip == true, we just discard the token and continue, as for whitespace and comments
		if (p_keep_nonsignificant || !skip)
		{
			// construct the token string from the range, if it has not already been set; the exception is
			// string tokens, which may be zero length at this point, and are already set up
			if ((token_type != EidosTokenType::kTokenString) && !token_string.size())
				token_string = script_string_.substr(token_start, token_end - token_start + 1);
			
			// figure out identifier-like tokens, which all get tokenized as kTokenIdentifier above
			if (token_type == EidosTokenType::kTokenIdentifier)
			{
				if (token_string.compare(gEidosStr_if) == 0) token_type = EidosTokenType::kTokenIf;
				else if (token_string.compare(gEidosStr_else) == 0) token_type = EidosTokenType::kTokenElse;
				else if (token_string.compare(gEidosStr_do) == 0) token_type = EidosTokenType::kTokenDo;
				else if (token_string.compare(gEidosStr_while) == 0) token_type = EidosTokenType::kTokenWhile;
				else if (token_string.compare(gEidosStr_for) == 0) token_type = EidosTokenType::kTokenFor;
				else if (token_string.compare(gEidosStr_in) == 0) token_type = EidosTokenType::kTokenIn;
				else if (token_string.compare(gEidosStr_next) == 0) token_type = EidosTokenType::kTokenNext;
				else if (token_string.compare(gEidosStr_break) == 0) token_type = EidosTokenType::kTokenBreak;
				else if (token_string.compare(gEidosStr_return) == 0) token_type = EidosTokenType::kTokenReturn;
				else if (token_string.compare(gEidosStr_function) == 0) token_type = EidosTokenType::kTokenFunction;
				
				// We used to modify the token string here for language keywords; we don't do that any more because it messed
				// up code completion, which assumes that the token string is a faithful copy of what matched the token.
				//if (token_type > EidosTokenType::kFirstIdentifierLikeToken)
				//	token_string = "<" + token_string + ">";
			}
			
			// make the token and push it
			token_stream_.emplace_back(token_type, token_string, token_start, token_end, token_UTF16_start, token_UTF16_end);
		}
		
		// advance to the character immediately past the end of this token
		pos = token_end + 1;
		pos_UTF16 = token_UTF16_end + 1;
	}
	
	// add an EOF token at the end
	token_stream_.emplace_back(EidosTokenType::kTokenEOF, "EOF", pos, pos, pos_UTF16, pos_UTF16);
	
	// if logging of tokens is requested, do that
	if (gEidosLogTokens)
	{
		std::cout << "Tokens : ";
		this->PrintTokens(std::cout);
	}
	
	// restore error tracking
	if (!p_make_bad_tokens)
		gEidosCurrentScript = current_script_save;
}

void EidosScript::Consume(void)
{
	// consume the token unless it is an EOF; we effectively have an infinite number of EOF tokens at the end
	if (current_token_type_ != EidosTokenType::kTokenEOF)
	{
		++parse_index_;
		current_token_ = &token_stream_.at(parse_index_);
		current_token_type_ = current_token_->token_type_;
	}
}

void EidosScript::Match(EidosTokenType p_token_type, const char *p_context_cstr)
{
	if (current_token_type_ == p_token_type)
	{
		// consume the token unless it is an EOF; we effectively have an infinite number of EOF tokens at the end
		if (current_token_type_ != EidosTokenType::kTokenEOF)
		{
			++parse_index_;
			current_token_ = &token_stream_.at(parse_index_);
			current_token_type_ = current_token_->token_type_;
		}
	}
	else
	{
		// if we are doing a fault-tolerant parse, just pretend we saw the token; otherwise, not finding the right token type is fatal
		if (!parse_make_bad_nodes_)
			EIDOS_TERMINATION << "ERROR (EidosScript::Match): unexpected token '" << *current_token_ << "' in " << std::string(p_context_cstr) << "; expected '" << p_token_type << "'." << EidosTerminate(current_token_);
	}
}

EidosASTNode *EidosScript::Parse_InterpreterBlock(bool p_allow_functions)
{
	EidosToken temp_token(EidosTokenType::kTokenInterpreterBlock, gEidosStr_empty_string, 0, 0, 0, 0);
	
	EidosASTNode *node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(&temp_token);	// the stack-local token is replaced below
	
	try
	{
		int32_t token_start = current_token_->token_start_;
		int32_t token_UTF16_start = current_token_->token_UTF16_start_;
		
		while (current_token_type_ != EidosTokenType::kTokenEOF)
		{
			EidosASTNode *child;
			
			// If p_allow_functions==T we're parsing a top-level interpreter block and function declarations are thus allowed.
			// If it is F, we are parsing something that is not a top-level block, and function declarations are not allowed.
			if (p_allow_functions && (current_token_type_ == EidosTokenType::kTokenFunction))
				child = Parse_FunctionDecl();
			else
				child = Parse_Statement();
			
			node->AddChild(child);
		}
		
		int32_t token_end = current_token_->token_start_ - 1;
		int32_t token_UTF16_end = current_token_->token_UTF16_start_ - 1;
		
		Match(EidosTokenType::kTokenEOF, "interpreter block");
		
		// swap in a new virtual token that encompasses all our children
		std::string &&token_string = script_string_.substr(token_start, token_end - token_start + 1);
		
		node->ReplaceTokenWithToken(new EidosToken(EidosTokenType::kTokenInterpreterBlock, token_string, token_start, token_end, token_UTF16_start, token_UTF16_end));
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_CompoundStatement(void)
{
	EidosASTNode *node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
	
	try
	{
		int32_t token_start = current_token_->token_start_;
		int32_t token_UTF16_start = current_token_->token_UTF16_start_;
		
		Match(EidosTokenType::kTokenLBrace, "compound statement");
		
		while ((current_token_type_ != EidosTokenType::kTokenRBrace) && (current_token_type_ != EidosTokenType::kTokenEOF))
		{
			EidosASTNode *child = Parse_Statement();
			
			node->AddChild(child);
		}
		
		int token_end = current_token_->token_start_;
		int32_t token_UTF16_end = current_token_->token_UTF16_start_;
		
		// We remember, with a flag, if we hit the EOF before seeing our end brace; this is used by EidosTypeInterpreter to know
		// what scope was active at the point when the parse ended, so it can leave the correct type table in place
		if (current_token_type_ == EidosTokenType::kTokenEOF)
			node->hit_eof_in_tolerant_parse_ = true;
		
		Match(EidosTokenType::kTokenRBrace, "compound statement");
		
		// swap in a new virtual token that encompasses all our children
		std::string &&token_string = script_string_.substr(token_start, token_end - token_start + 1);
		
		node->ReplaceTokenWithToken(new EidosToken(node->token_->token_type_, token_string, token_start, token_end, token_UTF16_start, token_UTF16_end));
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_Statement(void)
{
	if (current_token_type_ == EidosTokenType::kTokenLBrace)
		return Parse_CompoundStatement();
	else if (current_token_type_ == EidosTokenType::kTokenIf)
		return Parse_SelectionStatement();
	else if (current_token_type_ == EidosTokenType::kTokenDo)
		return Parse_DoWhileStatement();
	else if (current_token_type_ == EidosTokenType::kTokenWhile)
		return Parse_WhileStatement();
	else if (current_token_type_ == EidosTokenType::kTokenFor)
		return Parse_ForStatement();
	else if ((current_token_type_ == EidosTokenType::kTokenNext) || (current_token_type_ == EidosTokenType::kTokenBreak) || (current_token_type_ == EidosTokenType::kTokenReturn))
		return Parse_JumpStatement();
	else
	{
		if (parse_make_bad_nodes_)
		{
			// If we are doing a fault-tolerant parse, we need to guarantee that we don't get stuck in an infinite loop.
			// Various functions, such as Parse_InterpreterBlock() and Parse_CompoundStatement(), call this function
			// inside a loop and expect it to always advance the current token.  In the cases above, that is guaranteed;
			// the current token is some special token that will be matched.  In this case, however, it is not guaranteed;
			// a bad token like ',' will fail to be processed, we will get a bad node, and we will not advance.  In this
			// circumstance, we Consume() one token and return the bad token.  This method thus guarantees that it advances.
			
			// Note that all other loops in this parser loop conditional on the current token being of a particular type,
			// and then consume the current token because the loop knows that it matches.  All other loops besides the
			// loops that call Parse_Statement() are therefore guaranteed to terminate on bad input.  Many of the parsing
			// methods are not guaranteed to advance on bad input; many of them will end up generating a bad node and
			// not advancing.  That is OK – that is great – because somebody above them in the call stack *is* guaranteed
			// to advance, or at least to terminate, because of the above logic.  We may generate weird garbage, but we
			// will not hang, and for fault-tolerant parsing, that's about as much as you can reasonably expect.
			EidosToken *old_current_token = current_token_;
			EidosASTNode *expr_statement_node = Parse_ExprStatement();
			
			if (current_token_ == old_current_token)
				Consume();
			
			return expr_statement_node;
		}
		else
			return Parse_ExprStatement();
	}
}

EidosASTNode *EidosScript::Parse_ExprStatement(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		if (current_token_type_ == EidosTokenType::kTokenSemicolon)
		{
			// an empty statement is represented by a semicolon token; note that EOF cannot
			// substitute for this semicolon even when final_semicolon_optional_ is true
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			Consume();
		}
		else
		{
			node = Parse_AssignmentExpr();
			
			// match the terminating semicolon unless it is optional and we're at the EOF
			if (!final_semicolon_optional_ || (current_token_type_ != EidosTokenType::kTokenEOF))
				Match(EidosTokenType::kTokenSemicolon, "expression statement");
		}
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_SelectionStatement(void)
{
	EidosASTNode *node = nullptr, *test_expr, *true_statement, *false_statement;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenIf, "if statement");
		Match(EidosTokenType::kTokenLParen, "if statement");
		
		test_expr = Parse_Expr();
		node->AddChild(test_expr);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		node->full_range_end_token_ = current_token_;
#endif
		
		Match(EidosTokenType::kTokenRParen, "if statement");
		
		true_statement = Parse_Statement();
		node->AddChild(true_statement);
		
		if (current_token_type_ == EidosTokenType::kTokenElse)
		{
			Consume();
			
			false_statement = Parse_Statement();
			node->AddChild(false_statement);
		}
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_DoWhileStatement(void)
{
	EidosASTNode *node = nullptr, *test_expr, *statement;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		node->full_range_end_token_ = current_token_;
#endif
		
		Match(EidosTokenType::kTokenDo, "do/while statement");
		
		statement = Parse_Statement();
		node->AddChild(statement);
		
		Match(EidosTokenType::kTokenWhile, "do/while statement");
		Match(EidosTokenType::kTokenLParen, "do/while statement");
		
		test_expr = Parse_Expr();
		node->AddChild(test_expr);
		
		Match(EidosTokenType::kTokenRParen, "do/while statement");
		
		// match the terminating semicolon unless it is optional and we're at the EOF
		if (!final_semicolon_optional_ || (current_token_type_ != EidosTokenType::kTokenEOF))
			Match(EidosTokenType::kTokenSemicolon, "do/while statement");
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_WhileStatement(void)
{
	EidosASTNode *node = nullptr, *test_expr, *statement;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenWhile, "while statement");
		Match(EidosTokenType::kTokenLParen, "while statement");
		
		test_expr = Parse_Expr();
		node->AddChild(test_expr);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		node->full_range_end_token_ = current_token_;
#endif
		
		Match(EidosTokenType::kTokenRParen, "while statement");
		
		statement = Parse_Statement();
		node->AddChild(statement);
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_ForStatement(void)
{
	EidosASTNode *node = nullptr, *identifier, *range_expr, *statement;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenFor, "for statement");
		Match(EidosTokenType::kTokenLParen, "for statement");
		
		identifier = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		node->AddChild(identifier);
		
		Match(EidosTokenType::kTokenIdentifier, "for statement");
		Match(EidosTokenType::kTokenIn, "for statement");
		
		range_expr = Parse_Expr();
		node->AddChild(range_expr);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// PROFILING
		node->full_range_end_token_ = current_token_;
#endif
		
		Match(EidosTokenType::kTokenRParen, "for statement");
		
		statement = Parse_Statement();
		node->AddChild(statement);
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_JumpStatement(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		if ((current_token_type_ == EidosTokenType::kTokenNext) || (current_token_type_ == EidosTokenType::kTokenBreak))
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			Consume();
			
			// match the terminating semicolon unless it is optional and we're at the EOF
			if (!final_semicolon_optional_ || (current_token_type_ != EidosTokenType::kTokenEOF))
				Match(EidosTokenType::kTokenSemicolon, "next/break statement");
		}
		else if (current_token_type_ == EidosTokenType::kTokenReturn)
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			Consume();
			
			if ((current_token_type_ == EidosTokenType::kTokenSemicolon) || (current_token_type_ == EidosTokenType::kTokenEOF))
			{
				// match the terminating semicolon unless it is optional and we're at the EOF
				if (!final_semicolon_optional_ || (current_token_type_ != EidosTokenType::kTokenEOF))
					Match(EidosTokenType::kTokenSemicolon, "return statement");
			}
			else
			{
				EidosASTNode *value_expr = Parse_Expr();
				node->AddChild(value_expr);
				
				// match the terminating semicolon unless it is optional and we're at the EOF
				if (!final_semicolon_optional_ || (current_token_type_ != EidosTokenType::kTokenEOF))
					Match(EidosTokenType::kTokenSemicolon, "return statement");
			}
		}
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_Expr(void)
{
	return Parse_ConditionalExpr();
}

EidosASTNode *EidosScript::Parse_AssignmentExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_ConditionalExpr();
		
		if (current_token_type_ == EidosTokenType::kTokenAssign)
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			Consume();
			
			node->AddChild(Parse_ConditionalExpr());
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_ConditionalExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_LogicalOrExpr();
		
		if (current_token_type_ == EidosTokenType::kTokenConditional)
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			Consume();
			
			node->AddChild(Parse_ConditionalExpr());
			
			Match(EidosTokenType::kTokenElse, "ternary conditional expression");
			
			node->AddChild(Parse_ConditionalExpr());
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_LogicalOrExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_LogicalAndExpr();
		
		while (current_token_type_ == EidosTokenType::kTokenOr)
		{
			if (!node)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
			}
			Consume();
			
			node->AddChild(Parse_LogicalAndExpr());
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_LogicalAndExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_EqualityExpr();
		
		while (current_token_type_ == EidosTokenType::kTokenAnd)
		{
			if (!node)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
			}
			Consume();
			
			node->AddChild(Parse_EqualityExpr());
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_EqualityExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_RelationalExpr();
		
		while ((current_token_type_ == EidosTokenType::kTokenEq) || (current_token_type_ == EidosTokenType::kTokenNotEq))
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_RelationalExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_RelationalExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_AddExpr();
		
		while ((current_token_type_ == EidosTokenType::kTokenLt) || (current_token_type_ == EidosTokenType::kTokenGt) || (current_token_type_ == EidosTokenType::kTokenLtEq) || (current_token_type_ == EidosTokenType::kTokenGtEq))
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_AddExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_AddExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_MultExpr();
		
		while ((current_token_type_ == EidosTokenType::kTokenPlus) || (current_token_type_ == EidosTokenType::kTokenMinus))
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_MultExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_MultExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_SeqExpr();
		
		while ((current_token_type_ == EidosTokenType::kTokenMult) || (current_token_type_ == EidosTokenType::kTokenDiv) || (current_token_type_ == EidosTokenType::kTokenMod))
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_SeqExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_SeqExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_UnaryExpExpr();
		
		if (current_token_type_ == EidosTokenType::kTokenColon)
		{
			if (!node)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
			}
			Consume();
			
			node->AddChild(Parse_UnaryExpExpr());
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_UnaryExpExpr(void)
{
	// this merge of unary_expr and exp_expr was suggested by https://stackoverflow.com/a/53615462/2752221
	// it fixes a precedence problem with ^ and unary -, where -2^2 should be -(2^2) == -4 but came out as (-2)^2 == 4
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		if ((current_token_type_ == EidosTokenType::kTokenPlus) || (current_token_type_ == EidosTokenType::kTokenMinus) || (current_token_type_ == EidosTokenType::kTokenNot))
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			Consume();
			
			node->AddChild(Parse_UnaryExpExpr());
		}
		else
		{
			left_expr = Parse_PostfixExpr();
			
			if (current_token_type_ == EidosTokenType::kTokenExp)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
				
				Consume();
				
				node->AddChild(Parse_UnaryExpExpr());		// note right-associativity
			}
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_PostfixExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_PrimaryExpr();
		
		while (true)
		{
			if (current_token_type_ == EidosTokenType::kTokenLBracket)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
				
				Consume();
				
				// in Eidos 1.6 and later, we allow comma-separated sequences of subset expressions; we make dummy nodes for missing expressions
				// at the top of this loop, we are always expecting a subset expression, and need to make a dummy node if one is not present
				do
				{
					if (current_token_type_ == EidosTokenType::kTokenRBracket)
					{
						EidosASTNode *missing_expr_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
						
						node->AddChild(missing_expr_node);		// add a node representing the skipped expression; we use the ']' token for it
						
						break;
					}
					else if (current_token_type_ == EidosTokenType::kTokenComma)
					{
						EidosASTNode *missing_expr_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
						
						node->AddChild(missing_expr_node);		// add a node representing the skipped expression; we use the ',' token for it
						
						Match(EidosTokenType::kTokenComma, "postfix subset expression");
					}
					else
					{
						node->AddChild(Parse_Expr());
						
						// after a subset expression, we need to either finish the subset node, or get back to expecting an expression
						if (current_token_type_ == EidosTokenType::kTokenComma)
							Match(EidosTokenType::kTokenComma, "postfix subset expression");
						else if (current_token_type_ == EidosTokenType::kTokenRBracket)
							break;
						else
						{
							// If we're not fault-tolerant, we have an error.  If we are, we have to break out, because
							// we can't assume that the Parse_Expr() call above ends up consuming any tokens at all
							if (!parse_make_bad_nodes_)
								EIDOS_TERMINATION << "ERROR (EidosScript::Parse_PostfixExpr): unexpected token '" << *current_token_ << "'." << EidosTerminate(current_token_);
							else
								break;
						}
					}
				}
				while (current_token_type_ != EidosTokenType::kTokenEOF);
				
				// now we have reached our end bracket and can close up
				
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
				// PROFILING
				node->full_range_end_token_ = current_token_;
#endif
				
				Match(EidosTokenType::kTokenRBracket, "postfix subset expression");
				
				left_expr = node;
				node = nullptr;
			}
			else if (current_token_type_ == EidosTokenType::kTokenLParen)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
				
				Consume();
				
				if (current_token_type_ == EidosTokenType::kTokenRParen)
				{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
					// PROFILING
					node->full_range_end_token_ = current_token_;
#endif
					
					Consume();
				}
				else
				{
					Parse_ArgumentExprList(node);	// Parse_ArgumentExprList() adds the arguments directly to the function call node
					
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
					// PROFILING
					node->full_range_end_token_ = current_token_;
#endif
					
					Match(EidosTokenType::kTokenRParen, "postfix function call expression");
				}
				
				left_expr = node;
				node = nullptr;
			}
			else if (current_token_type_ == EidosTokenType::kTokenDot)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
				
				Consume();
				
				EidosASTNode *identifier = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
				node->AddChild(identifier);
				Match(EidosTokenType::kTokenIdentifier, "postfix member expression");
				
				left_expr = node;
				node = nullptr;
			}
			else
				break;
		}
	}
	catch (...)
	{
		if (left_expr)
		{
			left_expr->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(left_expr));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_PrimaryExpr(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		if ((current_token_type_ == EidosTokenType::kTokenNumber) || (current_token_type_ == EidosTokenType::kTokenString))
		{
			node = Parse_Constant();
		}
		else if (current_token_type_ == EidosTokenType::kTokenLParen)
		{
			Consume();
			
			node = Parse_Expr();
			
			Match(EidosTokenType::kTokenRParen, "primary parenthesized expression");
		}
		else if (current_token_type_ == EidosTokenType::kTokenIdentifier)
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			
			Match(EidosTokenType::kTokenIdentifier, "primary identifier expression");
		}
		else
		{
			if (!parse_make_bad_nodes_)
			{
				// Give a good error message if the user is using <function> as an identifier
				if (current_token_type_ == EidosTokenType::kTokenFunction)
					EIDOS_TERMINATION << "ERROR (EidosScript::Parse_PrimaryExpr): unexpected token '" << *current_token_ << "'.  Note that <function> is now an Eidos language keyword and can no longer be used as an identifier.  User-defined functions may only be declared at the top level, not inside blocks.  The parameter to doCall() is now named 'functionName', and the built-in function previously named 'function' is now named 'functionSignature'." << EidosTerminate(current_token_);
				else
					EIDOS_TERMINATION << "ERROR (EidosScript::Parse_PrimaryExpr): unexpected token '" << *current_token_ << "'." << EidosTerminate(current_token_);
			}
			
			// We're doing an error-tolerant parse, so we introduce a bad node here as a placeholder for a missing primary expression
			// BCH 5/29/2018: changing to use the position of current_token_, so the bad token has a known position here; this allows code completion
			// to suggest argument names inside functions and methods from an empty base.  See EidosTypeInterpreter::_ProcessArgumentListTypes().
			// BCH 4/6/2019: changing to take the full range of current_token here, so that completing off of a language keyword – really an identifier
			// that happens to match a language keyword – works properly in code completion.
			EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, current_token_->token_start_, current_token_->token_end_, current_token_->token_UTF16_start_, current_token_->token_UTF16_end_);
			EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
			
			node = bad_node;
		}
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

void EidosScript::Parse_ArgumentExprList(EidosASTNode *p_parent_node)
{
	p_parent_node->AddChild(Parse_ArgumentExpr());
	
	while (current_token_type_ == EidosTokenType::kTokenComma)
	{
		Consume();	// note that we no longer use EidosTokenType::kTokenComma in the AST to group call arguments
		
		p_parent_node->AddChild(Parse_ArgumentExpr());
	}
}

EidosASTNode *EidosScript::Parse_ArgumentExpr(void)
{
	EidosASTNode *identifier = nullptr, *node = nullptr;
	
	try {
		// Look ahead one token for the IDENTIFIER '=' pattern.  The token at parse_index_ + 1 will always be defined,
		// at least as an EOF; looking ahead two tokens would require a bounds check, but one token does not.
		if ((current_token_type_ == EidosTokenType::kTokenIdentifier) && (token_stream_.at(parse_index_ + 1).token_type_ == EidosTokenType::kTokenAssign))
		{
			identifier = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			
			Match(EidosTokenType::kTokenIdentifier, "argument expression identifier");
			
			if (current_token_type_ == EidosTokenType::kTokenAssign)
			{
				node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_, identifier);
				identifier = nullptr;
				Consume();
				
				node->AddChild(Parse_ConditionalExpr());
			}
		}
		else
		{
			return Parse_ConditionalExpr();
		}
	}
	catch (...)
	{
		if (identifier)
		{
			identifier->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(identifier));
		}
		
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return (node ? node : identifier);
}

EidosASTNode *EidosScript::Parse_Constant(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		if (current_token_type_ == EidosTokenType::kTokenNumber)
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			Match(EidosTokenType::kTokenNumber, "number literal expression");
		}
		else if (current_token_type_ == EidosTokenType::kTokenString)
		{
			node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			Match(EidosTokenType::kTokenString, "string literal expression");
		}
		else
		{
			// This case should actually never be hit, since Parse_Constant() is only called when we have already seen a number or string token
			if (!parse_make_bad_nodes_)
				EIDOS_TERMINATION << "ERROR (EidosScript::Parse_Constant): unexpected token '" << *current_token_ << "'." << EidosTerminate(current_token_);
			
			// We're doing an error-tolerant parse, so we introduce a bad node here as a placeholder for a missing constant
			EidosToken *bad_token = new EidosToken(EidosTokenType::kTokenBad, gEidosStr_empty_string, 0, 0, 0, 0);
			EidosASTNode *bad_node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(bad_token, true);
			
			node = bad_node;
		}
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_FunctionDecl(void)
{
	EidosASTNode *node = nullptr, *return_type, *identifier, *param_list, *body;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenFunction, "function declaration");
		
		return_type = Parse_ReturnTypeSpec();
		node->AddChild(return_type);
		
		// If we're doing a fault-tolerant parse and the next token is not an identifier, avoid putting garbage into the tree
		if (!parse_make_bad_nodes_ || (current_token_->token_type_ == EidosTokenType::kTokenIdentifier))
		{
			identifier = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			node->AddChild(identifier);
			Match(EidosTokenType::kTokenIdentifier, "function declaration");
		}
		
		param_list = Parse_ParamList();
		node->AddChild(param_list);
		
		body = Parse_CompoundStatement();
		node->AddChild(body);
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_ReturnTypeSpec()
{
	EidosASTNode *node = nullptr;
	
	try
	{
		Match(EidosTokenType::kTokenLParen, "return-type specifier");
		
		if (current_token_type_ == EidosTokenType::kTokenRParen)
			if (!parse_make_bad_nodes_)
				EIDOS_TERMINATION << "ERROR (EidosScript::Parse_ReturnTypeSpec): unexpected token '" << *current_token_ << "' in return-type specifier; perhaps 'void' is missing?  Note that function() has been renamed to functionSignature()." << EidosTerminate(current_token_);
		
		// create a node for the type-specifier
		node = Parse_TypeSpec();
		
		Match(EidosTokenType::kTokenRParen, "return-type specifier");
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_TypeSpec(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		node->typespec_.type_mask = kEidosValueMaskNone;
		node->typespec_.object_class = nullptr;
		
		if (current_token_type_ == EidosTokenType::kTokenIdentifier)
		{
			// Note that as a matter of syntax, this method will parse "void" and type-specifiers containing "v" as pertaining
			// to the "void" type in Eidos.  If the caller does not allow void to be used in a specific context, that is a
			// matter of semantics; the caller should check and raise if void is used, in that case.
			const std::string &type = current_token_->token_string_;
			
			if (type == "void")
			{
				node->typespec_.type_mask = kEidosValueMaskVOID;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
			}
			else if (type == "NULL")
			{
				node->typespec_.type_mask = kEidosValueMaskNULL;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
			}
			else if (type == "logical")
			{
				node->typespec_.type_mask = kEidosValueMaskLogical;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
			}
			else if (type == "integer")
			{
				node->typespec_.type_mask = kEidosValueMaskInt;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
			}
			else if (type == "float")
			{
				node->typespec_.type_mask = kEidosValueMaskFloat;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
			}
			else if (type == "string")
			{
				node->typespec_.type_mask = kEidosValueMaskString;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
			}
			else if (type == "object")
			{
				node->typespec_.type_mask = kEidosValueMaskObject;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
				
				if (current_token_type_ == EidosTokenType::kTokenLt)
					Parse_ObjectClassSpec(node);
			}
			else if (type == "numeric")
			{
				node->typespec_.type_mask = kEidosValueMaskNumeric;
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
			}
			else
			{
				// Check for a type composed of (vNlifso)*
				bool seen_v = false, seen_N = false, seen_l = false, seen_i = false, seen_f = false, seen_s = false, seen_o = false;
				bool saw_double = false;
				
				for (const char &c : type)
				{
					switch (c)
					{
						case 'v':	if (seen_v) saw_double = true; else seen_v = true; break;
						case 'N':	if (seen_N) saw_double = true; else seen_N = true; break;
						case 'l':	if (seen_l) saw_double = true; else seen_l = true; break;
						case 'i':	if (seen_i) saw_double = true; else seen_i = true; break;
						case 'f':	if (seen_f) saw_double = true; else seen_f = true; break;
						case 's':	if (seen_s) saw_double = true; else seen_s = true; break;
						case 'o':	if (seen_o) saw_double = true; else seen_o = true; break;
						default:	if (!parse_make_bad_nodes_)
										EIDOS_TERMINATION << "ERROR (EidosScript::Parse_TypeSpec): illegal type-specifier '" << type << "' (illegal character '" << c << "')." << EidosTerminate(current_token_);
					}
					
					if (saw_double)
						if (!parse_make_bad_nodes_)
							EIDOS_TERMINATION << "ERROR (EidosScript::Parse_TypeSpec): illegal type-specifier '" << type << "' (doubly specified type '" << c << "')." << EidosTerminate(current_token_);
				}
				
				if (seen_v)		node->typespec_.type_mask |= kEidosValueMaskVOID;
				if (seen_N)		node->typespec_.type_mask |= kEidosValueMaskNULL;
				if (seen_l)		node->typespec_.type_mask |= kEidosValueMaskLogical;
				if (seen_i)		node->typespec_.type_mask |= kEidosValueMaskInt;
				if (seen_f)		node->typespec_.type_mask |= kEidosValueMaskFloat;
				if (seen_s)		node->typespec_.type_mask |= kEidosValueMaskString;
				if (seen_o)		node->typespec_.type_mask |= kEidosValueMaskObject;
				
				Match(EidosTokenType::kTokenIdentifier, "type specifier");
				
				if (current_token_type_ == EidosTokenType::kTokenLt)
					Parse_ObjectClassSpec(node);
			}
		}
		else if (current_token_type_ == EidosTokenType::kTokenPlus)
		{
			// We just return a + node in this case; note it is different from a normal + node!
			node->typespec_.type_mask = kEidosValueMaskAnyBase;
			
			Match(EidosTokenType::kTokenPlus, "type specifier");
		}
		else if (current_token_type_ == EidosTokenType::kTokenMult)
		{
			// We just return a * node in this case; note it is different from a normal * node!
			node->typespec_.type_mask = kEidosValueMaskAny;
			
			Match(EidosTokenType::kTokenMult, "type specifier");
		}
		else
		{
			if (!parse_make_bad_nodes_)
				EIDOS_TERMINATION << "ERROR (EidosScript::Parse_TypeSpec): unexpected token '" << *current_token_ << "' in type specifier; expected a type identifier, +, or *." << EidosTerminate(current_token_);
		}
		
		if (current_token_type_ == EidosTokenType::kTokenSingleton)
		{
			// Check for some illegal cases, where (semantically) the type may not be declared as singleton
			if ((node->typespec_.type_mask == kEidosValueMaskVOID) ||
				(node->typespec_.type_mask == kEidosValueMaskNULL) ||
				(node->typespec_.type_mask == (kEidosValueMaskNULL | kEidosValueMaskVOID)))
				EIDOS_TERMINATION << "ERROR (EidosScript::Parse_TypeSpec): type-specifiers that consist only of void and/or NULL may not be declared to be singleton." << EidosTerminate(current_token_);
			
			// Mark the node as representing a singleton
			node->typespec_.type_mask |= kEidosValueMaskSingleton;
			
			Match(EidosTokenType::kTokenSingleton, "type specifier");
		}
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_ObjectClassSpec(EidosASTNode *p_type_node)
{
	try
	{
		Match(EidosTokenType::kTokenLt, "object-class specifier");
		
		const std::string &object_class = current_token_->token_string_;
		
		for (EidosObjectClass *eidos_class : gEidosContextClasses)
		{
			if (eidos_class->ElementType() == object_class)
			{
				p_type_node->typespec_.object_class = eidos_class;
				break;
			}
		}
		
		if (!p_type_node->typespec_.object_class)
			if (!parse_make_bad_nodes_)
				EIDOS_TERMINATION << "ERROR (EidosScript::Parse_ObjectClassSpec): could not find an Eidos class named '" << object_class << "')." << EidosTerminate(current_token_);
		
		Match(EidosTokenType::kTokenIdentifier, "object-class specifier");
		Match(EidosTokenType::kTokenGt, "object-class specifier");
	}
	catch (...)
	{
		throw;
	}
	
	return p_type_node;
}

EidosASTNode *EidosScript::Parse_ParamList(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenLParen, "parameter list");
		
		// Look ahead one token for the 'void' ')' pattern.  The token at parse_index_ + 1 will always be defined,
		// at least as an EOF; looking ahead two tokens would require a bounds check, but one token does not.
		if ((current_token_type_ == EidosTokenType::kTokenIdentifier) &&
			(current_token_->token_string_ == "void") &&
			(token_stream_.at(parse_index_ + 1).token_type_ == EidosTokenType::kTokenRParen))
		{
			// A void parameter list is represented by no children of the param-list node
			Match(EidosTokenType::kTokenIdentifier, "parameter list");
		}
		else
		{
			// Otherwise, each child represents one param-spec
			while (true)
			{
				EidosASTNode *param_spec = Parse_ParamSpec();
				
				node->AddChild(param_spec);
				
				if (current_token_type_ == EidosTokenType::kTokenComma)
				{
					Match(EidosTokenType::kTokenComma, "parameter list");
					continue;
				}
				
				break;
			}
		}
		
		Match(EidosTokenType::kTokenRParen, "parameter list");
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_ParamSpec(void)
{
	EidosASTNode *node = nullptr, *type_node, *parameter_id, *default_value;
	
	try
	{
		node = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
		
		if (current_token_type_ == EidosTokenType::kTokenLBracket)
		{
			// This is an optional argument of the form "[ type-spec ID = default-value ]"
			// It is stored as a node with three children: type-spec, ID, default-value
			// In this case the parent node is of type EidosTokenType::kTokenLBracket
			Match(EidosTokenType::kTokenLBracket, "parameter specifier");
			
			EidosToken *type_specifier_token = current_token_;
			
			type_node = Parse_TypeSpec();
			
			if (type_node->typespec_.type_mask & kEidosValueMaskVOID)
				EIDOS_TERMINATION << "ERROR (EidosScript::Parse_ParamSpec): void is not allowed in parameter type-specifiers; function parameters may not accept void arguments." << EidosTerminate(type_specifier_token);
			
			type_node->typespec_.type_mask |= kEidosValueMaskOptional;
			node->AddChild(type_node);
			
			parameter_id = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			node->AddChild(parameter_id);
			Match(EidosTokenType::kTokenIdentifier, "parameter specifier");
			
			Match(EidosTokenType::kTokenAssign, "parameter specifier");
			
			if (current_token_type_ == EidosTokenType::kTokenIdentifier)
			{
				default_value = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
				node->AddChild(default_value);
				Match(EidosTokenType::kTokenIdentifier, "parameter specifier");
			}
			else
			{
				default_value = Parse_Constant();
				node->AddChild(default_value);
			}
			
			Match(EidosTokenType::kTokenRBracket, "parameter specifier");
		}
		else
		{
			// This is a required argument of the form "type-spec ID"
			// It is stored as a node with two children: type-spec, ID
			// In this case the parent node is of type EidosTokenType::kTokenIdentifier
			EidosToken *type_specifier_token = current_token_;
			
			type_node = Parse_TypeSpec();
			
			if (type_node->typespec_.type_mask & kEidosValueMaskVOID)
				EIDOS_TERMINATION << "ERROR (EidosScript::Parse_ParamSpec): void is not allowed in parameter type-specifiers; function parameters may not accept void arguments." << EidosTerminate(type_specifier_token);
			
			node->AddChild(type_node);
			
			parameter_id = new (gEidosASTNodePool->AllocateChunk()) EidosASTNode(current_token_);
			node->AddChild(parameter_id);
			Match(EidosTokenType::kTokenIdentifier, "parameter specifier");
		}
	}
	catch (...)
	{
		if (node)
		{
			node->~EidosASTNode();
			gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(node));
		}
		
		throw;
	}
	
	return node;
}

void EidosScript::ParseInterpreterBlockToAST(bool p_allow_functions, bool p_make_bad_nodes)
{
	// destroy the parse root and return it to the pool; the tree must be allocated out of gEidosASTNodePool!
	if (parse_root_)
	{
		parse_root_->~EidosASTNode();
		gEidosASTNodePool->DisposeChunk(const_cast<EidosASTNode*>(parse_root_));
		parse_root_ = nullptr;
	}
	
	// set up parse state
	parse_index_ = 0;
	current_token_ = &token_stream_.at(parse_index_);		// should always have at least an EOF
	current_token_type_ = current_token_->token_type_;
	parse_make_bad_nodes_ = p_make_bad_nodes;
	
	// set up error tracking for this script
	EidosScript *current_script_save = gEidosCurrentScript;
	gEidosCurrentScript = this;
	
	// parse a new AST from our start token
	parse_root_ = Parse_InterpreterBlock(p_allow_functions);
	
	parse_root_->OptimizeTree();
	
	// if logging of the AST is requested, do that
	if (gEidosLogAST)
	{
		std::cout << "AST : \n";
		this->PrintAST(std::cout);
	}
	
	// restore error tracking
	gEidosCurrentScript = current_script_save;
	parse_make_bad_nodes_ = false;
}

void EidosScript::PrintTokens(std::ostream &p_outstream) const
{
	if (token_stream_.size())
	{
		for (auto &token : token_stream_)
			p_outstream << token << " ";
		
		p_outstream << std::endl;
	}
}

void EidosScript::PrintAST(std::ostream &p_outstream) const
{
	if (parse_root_)
	{
		parse_root_->PrintTreeWithIndent(p_outstream, 0);
		
		p_outstream << std::endl;
	}
}





























































