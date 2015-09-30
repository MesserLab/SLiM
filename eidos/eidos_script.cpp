//
//  eidos_script.cpp
//  Eidos
//
//  Created by Ben Haller on 4/1/15.
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


#include "eidos_script.h"
#include "eidos_global.h"
#include "eidos_value.h"
#include "eidos_interpreter.h"
#include "eidos_ast_node.h"

#include <iostream>
#include <sstream>


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::istream;
using std::ostream;


// set these to true to get logging of tokens / AST / evaluation
bool gEidosLogTokens = false;
bool gEidosLogAST = false;
bool gEidosLogEvaluation = false;


//
//	Script
//
#pragma mark Script

EidosScript::EidosScript(const string &p_script_string) :
	script_string_(p_script_string)
{
}

EidosScript::~EidosScript(void)
{
	for (auto token : token_stream_)
		delete token;
	
	delete parse_root_;
}

void EidosScript::Tokenize(bool p_make_bad_tokens, bool p_keep_nonsignificant)
{
	// set up error tracking for this script
	EidosScript *current_script_save = gEidosCurrentScript;
	gEidosCurrentScript = this;
	
	// delete all existing tokens, AST, etc.
	for (auto token : token_stream_)
		delete token;
	
	token_stream_.clear();
	
	delete parse_root_;
	parse_root_ = nullptr;
	
	// chew off one token at a time from script_string_, make a Token object, and add it
	int pos = 0, len = (int)script_string_.length();
	
	while (pos < len)
	{
		int token_start = pos;										// the first character position in the current token
		int token_end = pos;										// the last character position in the current token
		int ch = script_string_[pos];								// the current character
		int ch2 = ((pos >= len) ? 0 : script_string_[pos + 1]);		// look ahead one character
		bool skip = false;											// set to true to skip creating/adding this token
		EidosTokenType token_type = EidosTokenType::kTokenNone;
		string token_string;
		
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
			
			// cases that require lookahead due to ambiguity: =, <, >, !, /
			case '=':
				if (ch2 == '=') { token_type = EidosTokenType::kTokenEq; token_end++; }
				else { token_type = EidosTokenType::kTokenAssign; }
				break;
			case '<':	// <= or <
				if (ch2 == '=') { token_type = EidosTokenType::kTokenLtEq; token_end++; }
				else { token_type = EidosTokenType::kTokenLt; }
				break;
			case '>':	// >= or >
				if (ch2 == '=') { token_type = EidosTokenType::kTokenGtEq; token_end++; }
				else { token_type = EidosTokenType::kTokenGt; }
				break;
			case '!':	// != or !
				if (ch2 == '=') { token_type = EidosTokenType::kTokenNotEq; token_end++; }
				else { token_type = EidosTokenType::kTokenNot; }
				break;
			case '/':	// // or /
				if (ch2 == '/') {
					token_type = EidosTokenType::kTokenComment;
					auto newline_pos = script_string_.find_first_of("\n\r", token_start);
					if (newline_pos == string::npos)
						token_end = len - 1;
					else
						token_end = (int)newline_pos - 1;
					skip = true;
				}
				else { token_type = EidosTokenType::kTokenDiv; }
				break;
			
			// cases that require advancement: numbers, strings, identifiers, identifier-like tokens, whitespace
			default:
				if ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'))
				{
					// whitespace; any nonzero-length sequence of space, tab, \n, \r
					while (token_end + 1 < len)
					{
						int chn = script_string_[token_end + 1];
						
						if ((chn == ' ') || (chn == '\t') || (chn == '\n') || (chn == '\r'))
							token_end++;
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
						int chn = script_string_[token_end + 1];
						
						if ((chn >= '0') && (chn <= '9'))
							token_end++;
						else
							break;
					}
					
					// optional decimal sequence
					if ((token_end + 1 < len) && (script_string_[token_end + 1] == '.'))
					{
						token_end++;
						
						while (token_end + 1 < len)
						{
							int chn = script_string_[token_end + 1];
							
							if ((chn >= '0') && (chn <= '9'))
								token_end++;
							else
								break;
						}
					}
					
					// optional exponent sequence
					if ((token_end + 1 < len) && ((script_string_[token_end + 1] == 'e') || (script_string_[token_end + 1] == 'E')))
					{
						token_end++;
						
						// optional sign
						if ((token_end + 1 < len) && ((script_string_[token_end + 1] == '+') || (script_string_[token_end + 1] == '-')))
							token_end++;
						
						// mandatory exponent value; if this is missing, we drop through and token_type == EidosTokenType::kTokenNone
						if ((token_end + 1 < len) && ((script_string_[token_end + 1] >= '0') && (script_string_[token_end + 1] <= '9')))
						{
							while (token_end + 1 < len)
							{
								int chn = script_string_[token_end + 1];
								
								if ((chn >= '0') && (chn <= '9'))
									token_end++;
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
						int chn = script_string_[token_end + 1];
						
						if (((chn >= 'a') && (chn <= 'z')) || ((chn >= 'A') && (chn <= 'Z')) || ((chn >= '0') && (chn <= '9')) || (chn == '_'))
							token_end++;
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
						// during tokenization we don't treat the error position as a stack
						gEidosCharacterStartOfError = token_start;
						gEidosCharacterEndOfError = token_end;
						
						// unlike most other tokens, string literals do not terminate automatically at EOF or an illegal character
						if (token_end + 1 == len)
						{
							if (p_make_bad_tokens)
							{
								token_type = EidosTokenType::kTokenBad;
								break;
							}
							
							EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): unexpected EOF in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << eidos_terminate();
						}
						
						int chn = script_string_[token_end + 1];
						
						if (chn == (double_quoted ? '"' : '\''))
						{
							// end of string
							token_end++;
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
								
								EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): unexpected EOF in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << eidos_terminate();
							}
							
							char ch_esq = script_string_[token_end + 2];
							
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
							}
							else
							{
								// an illegal escape; if we are making bad tokens, we tolerate the error and continue
								if (p_make_bad_tokens)
								{
									token_string += ch_esq;
									token_end += 2;
								}
								else
								{
									gEidosCharacterStartOfError = token_end + 1;
									gEidosCharacterEndOfError = token_end + 2;
									
									EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): illegal escape \\" << (char)ch_esq << " in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << eidos_terminate();
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
							
							EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): illegal newline in string literal " << (double_quoted ? "\"" : "'") << token_string << (double_quoted ? "\"" : "'") << "." << eidos_terminate();
						}
						else
						{
							// all other characters get eaten up as part of the string literal
							token_string += (char)chn;
							token_end++;
						}
					}
					while (true);
				}
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
				
				EIDOS_TERMINATION << "ERROR (EidosScript::Tokenize): unrecognized token at character '" << (char)ch << "'." << eidos_terminate();
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
				
				// We used to modify the token string here for language keywords; we don't do that any more because it messed
				// up code completion, which assumes that the token string is a faithful copy of what matched the token.
				//if (token_type > EidosTokenType::kFirstIdentifierLikeToken)
				//	token_string = "<" + token_string + ">";
			}
			
			// make the token and push it
			EidosToken *token = new EidosToken(token_type, token_string, token_start, token_end);
			
			token_stream_.push_back(token);
		}
		
		// advance to the character immediately past the end of this token
		pos = token_end + 1;
	}
	
	// add an EOF token at the end
	EidosToken *eofToken = new EidosToken(EidosTokenType::kTokenEOF, "EOF", len, len);
	
	token_stream_.push_back(eofToken);
	
	// if logging of tokens is requested, do that
	if (gEidosLogTokens)
	{
		std::cout << "Tokens : ";
		this->PrintTokens(std::cout);
	}
	
	// restore error tracking
	gEidosCurrentScript = current_script_save;
}

void EidosScript::Consume(void)
{
	// consume the token unless it is an EOF; we effectively have an infinite number of EOF tokens at the end
	if (current_token_type_ != EidosTokenType::kTokenEOF)
	{
		++parse_index_;
		current_token_ = token_stream_.at(parse_index_);
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
			current_token_ = token_stream_.at(parse_index_);
			current_token_type_ = current_token_->token_type_;
		}
	}
	else
	{
		// not finding the right token type is fatal
		EIDOS_TERMINATION << "ERROR (EidosScript::Match): unexpected token '" << *current_token_ << "' in " << std::string(p_context_cstr) << "; expected '" << p_token_type << "'." << eidos_terminate(current_token_);
	}
}

EidosASTNode *EidosScript::Parse_InterpreterBlock(void)
{
	EidosToken *virtual_token = new EidosToken(EidosTokenType::kTokenInterpreterBlock, gEidosStr_empty_string, 0, 0);
	EidosASTNode *node = new EidosASTNode(virtual_token, true);
	
	try
	{
		int token_start = current_token_->token_start_;
		
		while (current_token_type_ != EidosTokenType::kTokenEOF)
		{
			EidosASTNode *child = Parse_Statement();
			
			node->AddChild(child);
		}
		
		int token_end = current_token_->token_start_ - 1;
		
		Match(EidosTokenType::kTokenEOF, "interpreter block");
		
		// swap in a new virtual token that encompasses all our children
		std::string &&token_string = script_string_.substr(token_start, token_end - token_start + 1);
		
		virtual_token = new EidosToken(EidosTokenType::kTokenInterpreterBlock, token_string, token_start, token_end);
		node->ReplaceTokenWithToken(virtual_token);
	}
	catch (...)
	{
		delete node;
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_CompoundStatement(void)
{
	EidosASTNode *node = new EidosASTNode(current_token_);
	
	try
	{
		int token_start = current_token_->token_start_;
		
		Match(EidosTokenType::kTokenLBrace, "compound statement");
		
		while (current_token_type_ != EidosTokenType::kTokenRBrace)
		{
			EidosASTNode *child = Parse_Statement();
			
			node->AddChild(child);
		}
		
		int token_end = current_token_->token_start_;
		
		Match(EidosTokenType::kTokenRBrace, "compound statement");
		
		// swap in a new virtual token that encompasses all our children
		std::string token_string = script_string_.substr(token_start, token_end - token_start + 1);
		
		EidosToken *virtual_token = new EidosToken(node->token_->token_type_, token_string, token_start, token_end);
		node->ReplaceTokenWithToken(virtual_token);
	}
	catch (...)
	{
		delete node;
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
		return Parse_ExprStatement();
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
			node = new EidosASTNode(current_token_);
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
		delete node;
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_SelectionStatement(void)
{
	EidosASTNode *node = nullptr, *test_expr, *true_statement, *false_statement;
	
	try
	{
		node = new EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenIf, "if statement");
		Match(EidosTokenType::kTokenLParen, "if statement");
		
		test_expr = Parse_Expr();
		node->AddChild(test_expr);
		
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
		delete node;
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_DoWhileStatement(void)
{
	EidosASTNode *node = nullptr, *test_expr, *statement;
	
	try
	{
		node = new EidosASTNode(current_token_);
		
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
		delete node;
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_WhileStatement(void)
{
	EidosASTNode *node = nullptr, *test_expr, *statement;
	
	try
	{
		node = new EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenWhile, "while statement");
		Match(EidosTokenType::kTokenLParen, "while statement");
		
		test_expr = Parse_Expr();
		node->AddChild(test_expr);
		
		Match(EidosTokenType::kTokenRParen, "while statement");
		
		statement = Parse_Statement();
		node->AddChild(statement);
	}
	catch (...)
	{
		delete node;
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_ForStatement(void)
{
	EidosASTNode *node = nullptr, *identifier, *range_expr, *statement;
	
	try
	{
		node = new EidosASTNode(current_token_);
		
		Match(EidosTokenType::kTokenFor, "for statement");
		Match(EidosTokenType::kTokenLParen, "for statement");
		
		identifier = new EidosASTNode(current_token_);
		node->AddChild(identifier);
		
		Match(EidosTokenType::kTokenIdentifier, "for statement");
		Match(EidosTokenType::kTokenIn, "for statement");
		
		range_expr = Parse_Expr();
		node->AddChild(range_expr);
		
		Match(EidosTokenType::kTokenRParen, "for statement");
		
		statement = Parse_Statement();
		node->AddChild(statement);
	}
	catch (...)
	{
		delete node;
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
			node = new EidosASTNode(current_token_);
			Consume();
			
			// match the terminating semicolon unless it is optional and we're at the EOF
			if (!final_semicolon_optional_ || (current_token_type_ != EidosTokenType::kTokenEOF))
				Match(EidosTokenType::kTokenSemicolon, "next/break statement");
		}
		else if (current_token_type_ == EidosTokenType::kTokenReturn)
		{
			node = new EidosASTNode(current_token_);
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
		delete node;
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_Expr(void)
{
	return Parse_LogicalOrExpr();
}

EidosASTNode *EidosScript::Parse_AssignmentExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_LogicalOrExpr();
		
		if (current_token_type_ == EidosTokenType::kTokenAssign)
		{
			node = new EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			Consume();
			
			node->AddChild(Parse_LogicalOrExpr());
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
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
				node = new EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
			}
			Consume();
			
			node->AddChild(Parse_LogicalAndExpr());
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
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
				node = new EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
			}
			Consume();
			
			node->AddChild(Parse_EqualityExpr());
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
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
			node = new EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_RelationalExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
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
			node = new EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_AddExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
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
			node = new EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_MultExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
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
			node = new EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_SeqExpr());
			
			left_expr = node;
			node = nullptr;
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_SeqExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_ExpExpr();
		
		if (current_token_type_ == EidosTokenType::kTokenColon)
		{
			if (!node)
			{
				node = new EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
			}
			Consume();
			
			node->AddChild(Parse_ExpExpr());
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_ExpExpr(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_UnaryExpr();
		
		if (current_token_type_ == EidosTokenType::kTokenExp)
		{
			node = new EidosASTNode(current_token_, left_expr);
			left_expr = nullptr;
			
			Consume();
			
			node->AddChild(Parse_ExpExpr());		// note right-associativity
			return node;
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
		throw;
	}
	
	return left_expr;
}

EidosASTNode *EidosScript::Parse_UnaryExpr(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		if ((current_token_type_ == EidosTokenType::kTokenPlus) || (current_token_type_ == EidosTokenType::kTokenMinus) || (current_token_type_ == EidosTokenType::kTokenNot))
		{
			node = new EidosASTNode(current_token_);
			Consume();
			
			node->AddChild(Parse_UnaryExpr());
		}
		else
		{
			node = Parse_PostfixExpr();
		}
	}
	catch (...)
	{
		delete node;
		throw;
	}
	
	return node;
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
				node = new EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
				
				Consume();
				
				node->AddChild(Parse_Expr());
				
				Match(EidosTokenType::kTokenRBracket, "postfix subset expression");
				
				left_expr = node;
				node = nullptr;
			}
			else if (current_token_type_ == EidosTokenType::kTokenLParen)
			{
				node = new EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
				
				Consume();
				
				if (current_token_type_ == EidosTokenType::kTokenRParen)
				{
					Consume();
				}
				else
				{
					node->AddChild(Parse_ArgumentExprList());
					
					Match(EidosTokenType::kTokenRParen, "postfix function call expression");
				}
				
				left_expr = node;
				node = nullptr;
			}
			else if (current_token_type_ == EidosTokenType::kTokenDot)
			{
				node = new EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
				
				Consume();
				
				EidosASTNode *identifier = new EidosASTNode(current_token_);
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
		delete left_expr;
		delete node;
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
			node = new EidosASTNode(current_token_);
			
			Match(EidosTokenType::kTokenIdentifier, "primary identifier expression");
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosScript::Parse_PrimaryExpr): unexpected token " << *current_token_ << "." << eidos_terminate(current_token_);
		}
	}
	catch (...)
	{
		delete node;
		throw;
	}
	
	return node;
}

EidosASTNode *EidosScript::Parse_ArgumentExprList(void)
{
	EidosASTNode *left_expr = nullptr, *node = nullptr;
	
	try
	{
		left_expr = Parse_AssignmentExpr();
		
		while (current_token_type_ == EidosTokenType::kTokenComma)
		{
			if (!node)
			{
				node = new EidosASTNode(current_token_, left_expr);
				left_expr = nullptr;
			}
			
			Consume();
			
			node->AddChild(Parse_AssignmentExpr());
		}
	}
	catch (...)
	{
		delete left_expr;
		delete node;
		throw;
	}
	
	return (node ? node : left_expr);
}

EidosASTNode *EidosScript::Parse_Constant(void)
{
	EidosASTNode *node = nullptr;
	
	try
	{
		if (current_token_type_ == EidosTokenType::kTokenNumber)
		{
			node = new EidosASTNode(current_token_);
			Match(EidosTokenType::kTokenNumber, "number literal expression");
		}
		else if (current_token_type_ == EidosTokenType::kTokenString)
		{
			node = new EidosASTNode(current_token_);
			Match(EidosTokenType::kTokenString, "string literal expression");
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (EidosScript::Parse_Constant): unexpected token " << *current_token_ << "." << eidos_terminate(current_token_);
		}
	}
	catch (...)
	{
		delete node;
		throw;
	}
	
	return node;
}

void EidosScript::ParseInterpreterBlockToAST(void)
{
	// delete the existing AST
	delete parse_root_;
	parse_root_ = nullptr;
	
	// set up parse state
	parse_index_ = 0;
	current_token_ = token_stream_.at(parse_index_);		// should always have at least an EOF
	current_token_type_ = current_token_->token_type_;
	
	// set up error tracking for this script
	EidosScript *current_script_save = gEidosCurrentScript;
	gEidosCurrentScript = this;
	
	// parse a new AST from our start token
	parse_root_ = Parse_InterpreterBlock();
	
	parse_root_->OptimizeTree();
	
	// if logging of the AST is requested, do that
	if (gEidosLogAST)
	{
		std::cout << "AST : \n";
		this->PrintAST(std::cout);
	}
	
	// restore error tracking
	gEidosCurrentScript = current_script_save;
}

void EidosScript::PrintTokens(ostream &p_outstream) const
{
	if (token_stream_.size())
	{
		for (auto token : token_stream_)
			p_outstream << *token << " ";
		
		p_outstream << endl;
	}
}

void EidosScript::PrintAST(ostream &p_outstream) const
{
	if (parse_root_)
	{
		parse_root_->PrintTreeWithIndent(p_outstream, 0);
		
		p_outstream << endl;
	}
}





























































