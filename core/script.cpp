//
//  script.cpp
//  SLiM
//
//  Created by Ben Haller on 4/1/15.
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


#include "script.h"
#include "slim_global.h"
#include "slim_sim.h"

#include <iostream>
#include <sstream>


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::istream;
using std::ostream;


// set these to true to get logging of tokens / AST / evaluation
bool gSLiMScriptLogTokens = false;
bool gSLiMScriptLogAST = false;
bool gSLiMScriptLogEvaluation = false;


std::ostream &operator<<(std::ostream &p_outstream, const TokenType p_token_type)
{
	switch (p_token_type)
	{
		case TokenType::kTokenNone:			p_outstream << "NO_TOKEN";		break;
		case TokenType::kTokenEOF:			p_outstream << "EOF";			break;
		case TokenType::kTokenWhitespace:	p_outstream << "WS";			break;
		case TokenType::kTokenSemicolon:	p_outstream << ";";				break;
		case TokenType::kTokenColon:		p_outstream << ":";				break;
		case TokenType::kTokenComma:		p_outstream << ",";				break;
		case TokenType::kTokenLBrace:		p_outstream << "{";				break;
		case TokenType::kTokenRBrace:		p_outstream << "}";				break;
		case TokenType::kTokenLParen:		p_outstream << "(";				break;
		case TokenType::kTokenRParen:		p_outstream << ")";				break;
		case TokenType::kTokenLBracket:		p_outstream << "[";				break;
		case TokenType::kTokenRBracket:		p_outstream << "]";				break;
		case TokenType::kTokenDot:			p_outstream << ".";				break;
		case TokenType::kTokenPlus:			p_outstream << "+";				break;
		case TokenType::kTokenMinus:		p_outstream << "-";				break;
		case TokenType::kTokenMod:			p_outstream << "%";				break;
		case TokenType::kTokenMult:			p_outstream << "*";				break;
		case TokenType::kTokenExp:			p_outstream << "^";				break;
		case TokenType::kTokenAnd:			p_outstream << "&";				break;
		case TokenType::kTokenOr:			p_outstream << "|";				break;
		case TokenType::kTokenDiv:			p_outstream << "/";				break;
		case TokenType::kTokenComment:		p_outstream << "COMMENT";		break;
		case TokenType::kTokenAssign:		p_outstream << "=";				break;
		case TokenType::kTokenEq:			p_outstream << "==";			break;
		case TokenType::kTokenLt:			p_outstream << "<";				break;
		case TokenType::kTokenLtEq:			p_outstream << "<=";			break;
		case TokenType::kTokenGt:			p_outstream << ">";				break;
		case TokenType::kTokenGtEq:			p_outstream << ">=";			break;
		case TokenType::kTokenNot:			p_outstream << "!";				break;
		case TokenType::kTokenNotEq:		p_outstream << "!=";			break;
		case TokenType::kTokenNumber:		p_outstream << "NUMBER";		break;
		case TokenType::kTokenString:		p_outstream << "STRING";		break;
		case TokenType::kTokenIdentifier:	p_outstream << "IDENTIFIER";	break;
		case TokenType::kTokenIf:			p_outstream << gStr_if;			break;
		case TokenType::kTokenElse:			p_outstream << gStr_else;			break;
		case TokenType::kTokenDo:			p_outstream << gStr_do;			break;
		case TokenType::kTokenWhile:		p_outstream << gStr_while;			break;
		case TokenType::kTokenFor:			p_outstream << gStr_for;			break;
		case TokenType::kTokenIn:			p_outstream << gStr_in;			break;
		case TokenType::kTokenNext:			p_outstream << gStr_next;			break;
		case TokenType::kTokenBreak:		p_outstream << gStr_break;			break;
		case TokenType::kTokenReturn:		p_outstream << gStr_return;		break;
			
		case TokenType::kTokenFitness:		p_outstream << gStr_fitness;		break;
		case TokenType::kTokenMateChoice:	p_outstream << gStr_mateChoice;	break;
		case TokenType::kTokenModifyChild:	p_outstream << gStr_modifyChild;	break;
			
		case TokenType::kTokenInterpreterBlock:		p_outstream << "$>";	break;
		case TokenType::kTokenSLiMFile:				p_outstream << "###";	break;
		case TokenType::kTokenSLiMScriptBlock:		p_outstream << "#>";	break;
		case TokenType::kFirstIdentifierLikeToken:	p_outstream << "???";	break;
	}
	
	return p_outstream;
}


//
//	ScriptToken
//
#pragma mark ScriptToken

ScriptToken::ScriptToken(TokenType p_token_type, string p_token_string, int p_token_start, int p_token_end) :
	token_type_(p_token_type), token_string_(p_token_string), token_start_(p_token_start), token_end_(p_token_end)
{
}

ostream &operator<<(ostream &p_outstream, const ScriptToken &p_token)
{
	// print strings, identifiers, numbers, and keywords with identifying marks; apart from that, print tokens as is
	if (p_token.token_type_ == TokenType::kTokenString)
		p_outstream << "\"" << p_token.token_string_ << "\"";
	else if (p_token.token_type_ == TokenType::kTokenIdentifier)
		p_outstream << "@" << p_token.token_string_;
	else if (p_token.token_type_ == TokenType::kTokenNumber)
		p_outstream << "#" << p_token.token_string_;
	else if (p_token.token_type_ > TokenType::kFirstIdentifierLikeToken)
		p_outstream << p_token.token_string_;	// includes <> delimiters
	else
		p_outstream << p_token.token_type_;
	
	return p_outstream;
}


//
//	ScriptASTNode
//
#pragma mark ScriptASTNode

ScriptASTNode::ScriptASTNode(ScriptToken *p_token, bool p_token_is_owned) :
	token_(p_token), token_is_owned_(p_token_is_owned)
{
}

ScriptASTNode::ScriptASTNode(ScriptToken *p_token, ScriptASTNode *p_child_node) :
	token_(p_token)
{
	this->AddChild(p_child_node);
}

ScriptASTNode::~ScriptASTNode(void)
{
	for (auto child : children_)
		delete child;
	
	if (cached_value_)
	{
		if (cached_value_is_owned_)
			delete cached_value_;
		
		cached_value_ = nullptr;
	}
	
	if (token_is_owned_)
	{
		delete token_;
		token_ = nullptr;
	}
}

void ScriptASTNode::AddChild(ScriptASTNode *p_child_node)
{
	children_.push_back(p_child_node);
}

void ScriptASTNode::ReplaceTokenWithToken(ScriptToken *p_token)
{
	// dispose of our previous token
	if (token_is_owned_)
	{
		delete token_;
		token_ = nullptr;
		token_is_owned_ = false;
	}
	
	// used to fix virtual token to encompass their children; takes ownership
	token_ = p_token;
	token_is_owned_ = true;
}

void ScriptASTNode::PrintToken(std::ostream &p_outstream) const
{
	// We want to print some tokens differently when they are in the context of an AST, for readability
	string token_string;
	
	switch (token_->token_type_)
	{
		case TokenType::kTokenLBrace: token_string = "BLOCK"; break;
		case TokenType::kTokenSemicolon: token_string = "NULL_STATEMENT"; break;
		case TokenType::kTokenLParen: token_string = "CALL"; break;
		case TokenType::kTokenLBracket: token_string = "SUBSET"; break;
		case TokenType::kTokenComma: token_string = "ARG_LIST"; break;
		default: break;
	}
	
	if (token_string.length())
		p_outstream << token_string;
	else
		p_outstream << *token_;
}

void ScriptASTNode::PrintTreeWithIndent(std::ostream &p_outstream, int p_indent) const
{
	// If we are indented, start a new line and indent
	if (p_indent > 0)
	{
		p_outstream << "\n  ";
		
		for (int i = 0; i < p_indent - 1; ++i)
			p_outstream << "  ";
	}
	
	if (children_.size() == 0)
	{
		// If we are a leaf, just print our token
		PrintToken(p_outstream);
	}
	else
	{
		// Determine whether we have only leaves as children
		bool childWithChildren = false;
		
		for (auto child : children_)
		{
			if (child->children_.size() > 0)
			{
				childWithChildren = true;
				break;
			}
		}
		
		if (childWithChildren)
		{
			// If we have non-leaf children, then print them with an incremented indent
			p_outstream << "(";
			PrintToken(p_outstream);
			
			for (auto child : children_)
				child->PrintTreeWithIndent(p_outstream, p_indent + 1);
			
			// and then outdent and show our end paren
			p_outstream << "\n";
			
			if (p_indent > 0)
			{
				p_outstream << "  ";
				
				for (int i = 0; i < p_indent - 1; ++i)
					p_outstream << "  ";
			}
			
			p_outstream << ")";
		}
		else
		{
			// If we have only leaves as children, then print everything on one line, for compactness
			p_outstream << "(";
			PrintToken(p_outstream);
			
			for (auto child : children_)
			{
				p_outstream << " ";
				child->PrintToken(p_outstream);
			}
			
			p_outstream << ")";
		}
	}
}


//
//	Script
//
#pragma mark Script

Script::Script(string p_script_string, int p_start_index) :
	script_string_(p_script_string), start_character_index_(p_start_index)
{
}

Script::~Script(void)
{
	for (auto token : token_stream_)
		delete token;
	
	delete parse_root_;
}

void Script::Tokenize(bool p_keep_nonsignificant)
{
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
		TokenType token_type = TokenType::kTokenNone;
		string token_string;
		
		switch (ch)
		{
			// cases that require just a single character to match
			case ';': token_type = TokenType::kTokenSemicolon; break;
			case ':': token_type = TokenType::kTokenColon; break;
			case ',': token_type = TokenType::kTokenComma; break;
			case '{': token_type = TokenType::kTokenLBrace; break;
			case '}': token_type = TokenType::kTokenRBrace; break;
			case '(': token_type = TokenType::kTokenLParen; break;
			case ')': token_type = TokenType::kTokenRParen; break;
			case '[': token_type = TokenType::kTokenLBracket; break;
			case ']': token_type = TokenType::kTokenRBracket; break;
			case '.': token_type = TokenType::kTokenDot; break;
			case '+': token_type = TokenType::kTokenPlus; break;
			case '-': token_type = TokenType::kTokenMinus; break;
			case '*': token_type = TokenType::kTokenMult; break;
			case '^': token_type = TokenType::kTokenExp; break;
			case '%': token_type = TokenType::kTokenMod; break;
			case '&': token_type = TokenType::kTokenAnd; break;
			case '|': token_type = TokenType::kTokenOr; break;
			
			// cases that require lookahead due to ambiguity: =, <, >, !, /
			case '=':
				if (ch2 == '=') { token_type = TokenType::kTokenEq; token_end++; }
				else { token_type = TokenType::kTokenAssign; }
				break;
			case '<':	// <= or <
				if (ch2 == '=') { token_type = TokenType::kTokenLtEq; token_end++; }
				else { token_type = TokenType::kTokenLt; }
				break;
			case '>':	// >= or >
				if (ch2 == '=') { token_type = TokenType::kTokenGtEq; token_end++; }
				else { token_type = TokenType::kTokenGt; }
				break;
			case '!':	// != or !
				if (ch2 == '=') { token_type = TokenType::kTokenNotEq; token_end++; }
				else { token_type = TokenType::kTokenNot; }
				break;
			case '/':	// // or /
				if (ch2 == '/') {
					token_type = TokenType::kTokenComment;
					auto newline_pos = script_string_.find_first_of("\n\r", token_start);
					if (newline_pos == string::npos)
						token_end = len - 1;
					else
						token_end = (int)newline_pos - 1;
					skip = true;
				}
				else { token_type = TokenType::kTokenDiv; }
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
					
					token_type = TokenType::kTokenWhitespace;
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
						
						// mandatory exponent value; if this is missing, we drop through and token_type == TokenType::kTokenNone
						if ((token_end + 1 < len) && ((script_string_[token_end + 1] >= '0') || (script_string_[token_end + 1] <= '9')))
						{
							while (token_end + 1 < len)
							{
								int chn = script_string_[token_end + 1];
								
								if ((chn >= '0') && (chn <= '9'))
									token_end++;
								else
									break;
							}
							
							token_type = TokenType::kTokenNumber;
						}
					}
					else
					{
						token_type = TokenType::kTokenNumber;
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
					
					token_type = TokenType::kTokenIdentifier;
				}
				else if (ch == '"')
				{
					// string literal: bounded by double quotes, with escapes (\t, \r, \n, \", \\), newlines not allowed
					do 
					{
						gCharacterStartOfParseError = start_character_index_ + token_start;
						gCharacterEndOfParseError = start_character_index_ + token_end;
						
						// unlike most other tokens, string literals do not terminate automatically at EOF or an illegal character
						if (token_end + 1 == len)
							SLIM_TERMINATION << "ERROR (Tokenize): unexpected EOF in string literal \"" << token_string << "\"" << slim_terminate();
						
						int chn = script_string_[token_end + 1];
						
						if (chn == '"')
						{
							// end of string
							token_end++;
							break;
						}
						else if (chn == '\\')
						{
							// escape sequence; another character must exist
							if (token_end + 2 == len)
								SLIM_TERMINATION << "ERROR (Tokenize): unexpected EOF in string literal \"" << token_string << "\"" << slim_terminate();
							
							int ch_esq = script_string_[token_end + 2];
							
							if ((ch_esq == 't') || (ch_esq == 'r') || (ch_esq == 'n') || (ch_esq == '"') || (ch_esq == '\\'))
							{
								// a legal escape, so replace it with the escaped character here
								if (ch_esq == 't')			token_string += '\t';
								else if (ch_esq == 'r')		token_string += '\r';
								else if (ch_esq == 'n')		token_string += '\n';
								else if (ch_esq == '"')		token_string += '"';
								else if (ch_esq == '\\')	token_string += '\\';
								token_end += 2;
							}
							else
							{
								// an illegal escape
								SLIM_TERMINATION << "ERROR (Tokenize): illegal escape \\" << (char)ch_esq << " in string literal \"" << token_string << "\"" << slim_terminate();
							}
						}
						else if ((chn == '\n') || (chn == '\r'))
						{
							// literal newlines are not allowed within string literals at present
							SLIM_TERMINATION << "ERROR (Tokenize): illegal newline in string literal \"" << token_string << "\"" << slim_terminate();
						}
						else
						{
							// all other characters get eaten up as part of the string literal
							token_string += (char)chn;
							token_end++;
						}
					}
					while (true);
					
					token_type = TokenType::kTokenString;
				}
				break;
		}
		
		if (token_type == TokenType::kTokenNone)
		{
			// failed to find a match; this causes a syntax error raise
			gCharacterStartOfParseError = start_character_index_ + token_start;
			gCharacterEndOfParseError = start_character_index_ + token_end;
			
			SLIM_TERMINATION << "ERROR (Tokenize): unrecognized token at character '" << (char)ch << "'" << slim_terminate();
		}
		
		// if skip == true, we just discard the token and continue, as for whitespace and comments
		if (p_keep_nonsignificant || !skip)
		{
			// construct the token string from the range, if it has not already been set; the exception is
			// string tokens, which may be zero length at this point, and are already set up
			if ((token_type != TokenType::kTokenString) && !token_string.length())
				token_string = script_string_.substr(token_start, token_end - token_start + 1);
			
			// figure out identifier-like tokens, which all get tokenized as kTokenIdentifier above
			if (token_type == TokenType::kTokenIdentifier)
			{
				if (token_string.compare(gStr_if) == 0) token_type = TokenType::kTokenIf;
				else if (token_string.compare(gStr_else) == 0) token_type = TokenType::kTokenElse;
				else if (token_string.compare(gStr_do) == 0) token_type = TokenType::kTokenDo;
				else if (token_string.compare(gStr_while) == 0) token_type = TokenType::kTokenWhile;
				else if (token_string.compare(gStr_for) == 0) token_type = TokenType::kTokenFor;
				else if (token_string.compare(gStr_in) == 0) token_type = TokenType::kTokenIn;
				else if (token_string.compare(gStr_next) == 0) token_type = TokenType::kTokenNext;
				else if (token_string.compare(gStr_break) == 0) token_type = TokenType::kTokenBreak;
				else if (token_string.compare(gStr_return) == 0) token_type = TokenType::kTokenReturn;
				
				// SLiM keywords
				else if (token_string.compare(gStr_fitness) == 0) token_type = TokenType::kTokenFitness;
				else if (token_string.compare(gStr_mateChoice) == 0) token_type = TokenType::kTokenMateChoice;
				else if (token_string.compare(gStr_modifyChild) == 0) token_type = TokenType::kTokenModifyChild;
				
				if (token_type > TokenType::kFirstIdentifierLikeToken)
					token_string = gStr_lessThanSign + token_string + gStr_greaterThanSign;
			}
			
			// make the token and push it
			ScriptToken *token = new ScriptToken(token_type, token_string, token_start, token_end);
			
			token_stream_.push_back(token);
		}
		
		// advance to the character immediately past the end of this token
		pos = token_end + 1;
	}
	
	// add an EOF token at the end
	ScriptToken *eofToken = new ScriptToken(TokenType::kTokenEOF, "EOF", len, len);
	
	token_stream_.push_back(eofToken);
	
	// if logging of tokens is requested, do that; always to cout, not to SLIM_OUTSTREAM
	if (gSLiMScriptLogTokens)
	{
		std::cout << "Tokens : ";
		this->PrintTokens(std::cout);
	}
}

void Script::AddOptionalSemicolon(void)
{
	auto eof_iter = token_stream_.end();
	int eof_start = INT_MAX;
	
	for (auto token_iter = token_stream_.end() - 1; token_iter != token_stream_.begin(); --token_iter)
	{
		ScriptToken *token = *token_iter;
		
		// if we see an EOF, we want to remember it so we can insert before it and use its position
		if (token->token_type_ == TokenType::kTokenEOF)
		{
			eof_iter = token_iter;
			eof_start = token->token_start_;
			continue;
		}
		
		// if we see a '}' or ';' token, the token stream is correctly terminated (or at least we can't fix it)
		if (token->token_type_ == TokenType::kTokenRBrace)
			return;
		if (token->token_type_ == TokenType::kTokenSemicolon)
			return;
		
		break;
	}
	
	if (eof_iter != token_stream_.end())
	{
		ScriptToken *virtual_semicolon = new ScriptToken(TokenType::kTokenSemicolon, ";", eof_start, eof_start);
		
		token_stream_.insert(eof_iter, virtual_semicolon);
	}
}

void Script::Consume()
{
	// consume the token unless it is an EOF; we effectively have an infinite number of EOF tokens at the end
	if (current_token_type_ != TokenType::kTokenEOF)
	{
		++parse_index_;
		current_token_ = token_stream_.at(parse_index_);
		current_token_type_ = current_token_->token_type_;
	}
}

void Script::SetErrorPositionFromCurrentToken(void)
{
	gCharacterStartOfParseError = start_character_index_ + current_token_->token_start_;
	gCharacterEndOfParseError = start_character_index_ + current_token_->token_end_;
}

void Script::Match(TokenType p_token_type, const char *p_context_cstr)
{
	if (current_token_type_ == p_token_type)
	{
		// consume the token unless it is an EOF; we effectively have an infinite number of EOF tokens at the end
		if (current_token_type_ != TokenType::kTokenEOF)
		{
			++parse_index_;
			current_token_ = token_stream_.at(parse_index_);
			current_token_type_ = current_token_->token_type_;
		}
	}
	else
	{
		// not finding the right token type is fatal
		SetErrorPositionFromCurrentToken();
		SLIM_TERMINATION << "ERROR (Parse): unexpected token '" << *current_token_ << "' in " << std::string(p_context_cstr) << "; expected '" << p_token_type << "'" << slim_terminate();
	}
}

ScriptASTNode *Script::Parse_SLiMFile(void)
{
	ScriptToken *virtual_token = new ScriptToken(TokenType::kTokenSLiMFile, gStr_empty_string, 0, 0);
	ScriptASTNode *node = new ScriptASTNode(virtual_token, true);
	
	while (current_token_type_ != TokenType::kTokenEOF)
	{
		// We handle the grammar a bit differently than how it is printed in the railroad diagrams in the doc.
		// Parsing of the optional generation range is done in Parse_SLiMScriptBlock() since it ends up as children of that node.
		ScriptASTNode *script_block = Parse_SLiMScriptBlock();
		
		node->AddChild(script_block);
	}
	
	Match(TokenType::kTokenEOF, "SLiM file");
	
	return node;
}

ScriptASTNode *Script::Parse_SLiMScriptBlock(void)
{
	ScriptToken *virtual_token = new ScriptToken(TokenType::kTokenSLiMScriptBlock, gStr_empty_string, 0, 0);
	ScriptASTNode *slim_script_block_node = new ScriptASTNode(virtual_token, true);
	
	// We handle the grammar a bit differently than how it is printed in the railroad diagrams in the doc.
	// We parse the slim_script_info section here, as part of the script block.
	if (current_token_type_ == TokenType::kTokenString)
	{
		// a script identifier string is present; add it
		ScriptASTNode *script_id_node = Parse_Constant();
		
		slim_script_block_node->AddChild(script_id_node);
	}
	
	if (current_token_type_ == TokenType::kTokenNumber)
	{
		// A start generation is present; add it
		ScriptASTNode *start_generation_node = Parse_Constant();
		
		slim_script_block_node->AddChild(start_generation_node);
		
		if (current_token_type_ == TokenType::kTokenColon)
		{
			// An end generation is present; add it
			Match(TokenType::kTokenColon, "SLiM script block");
			
			if (current_token_type_ == TokenType::kTokenNumber)
			{
				ScriptASTNode *end_generation_node = Parse_Constant();
				
				slim_script_block_node->AddChild(end_generation_node);
			}
			else
			{
				SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_SLiMScriptBlock" << slim_terminate();
			}
		}
	}
	
	// Now we are to the point of parsing the actual slim_script_block
	if (current_token_type_ == TokenType::kTokenFitness)
	{
		ScriptASTNode *callback_info_node = new ScriptASTNode(current_token_);
		
		Match(TokenType::kTokenFitness, "SLiM fitness() callback");
		Match(TokenType::kTokenLParen, "SLiM fitness() callback");
		
		if (current_token_type_ == TokenType::kTokenNumber)
		{
			// A (required) mutation type id is present; add it
			ScriptASTNode *mutation_type_id_node = Parse_Constant();
			
			callback_info_node->AddChild(mutation_type_id_node);
		}
		else
		{
			SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_SLiMScriptBlock; a mutation type id is required in fitness() callback definitions" << slim_terminate();
		}
		
		if (current_token_type_ == TokenType::kTokenComma)
		{
			// A (optional) subpopulation id is present; add it
			Match(TokenType::kTokenComma, "SLiM fitness() callback");
			
			if (current_token_type_ == TokenType::kTokenNumber)
			{
				ScriptASTNode *subpopulation_id_node = Parse_Constant();
				
				callback_info_node->AddChild(subpopulation_id_node);
			}
			else
			{
				SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_SLiMScriptBlock; a subpopulation id is expected after a comma in fitness() callback definitions" << slim_terminate();
			}
		}
		
		Match(TokenType::kTokenRParen, "SLiM fitness() callback");
		
		slim_script_block_node->AddChild(callback_info_node);
	}
	else if (current_token_type_ == TokenType::kTokenMateChoice)
	{
		ScriptASTNode *callback_info_node = new ScriptASTNode(current_token_);
		
		Match(TokenType::kTokenMateChoice, "SLiM mateChoice() callback");
		Match(TokenType::kTokenLParen, "SLiM mateChoice() callback");
		
		// A (optional) subpopulation id is present; add it
		if (current_token_type_ == TokenType::kTokenNumber)
		{
			ScriptASTNode *subpopulation_id_node = Parse_Constant();
			
			callback_info_node->AddChild(subpopulation_id_node);
		}
		
		Match(TokenType::kTokenRParen, "SLiM mateChoice() callback");
		
		slim_script_block_node->AddChild(callback_info_node);
	}
	else if (current_token_type_ == TokenType::kTokenModifyChild)
	{
		ScriptASTNode *callback_info_node = new ScriptASTNode(current_token_);
		
		Match(TokenType::kTokenModifyChild, "SLiM modifyChild() callback");
		Match(TokenType::kTokenLParen, "SLiM modifyChild() callback");
		
		// A (optional) subpopulation id is present; add it
		if (current_token_type_ == TokenType::kTokenNumber)
		{
			ScriptASTNode *subpopulation_id_node = Parse_Constant();
			
			callback_info_node->AddChild(subpopulation_id_node);
		}
		
		Match(TokenType::kTokenRParen, "SLiM modifyChild() callback");
		
		slim_script_block_node->AddChild(callback_info_node);
	}
	
	// Regardless of what happened above, all SLiMscript blocks end with a compound statement, which is the last child of the node
	ScriptASTNode *compound_statement_node = Parse_CompoundStatement();
	
	slim_script_block_node->AddChild(compound_statement_node);
	
	return slim_script_block_node;
}

ScriptASTNode *Script::Parse_InterpreterBlock(void)
{
	ScriptToken *virtual_token = new ScriptToken(TokenType::kTokenInterpreterBlock, gStr_empty_string, 0, 0);
	ScriptASTNode *node = new ScriptASTNode(virtual_token, true);
	
	int token_start = current_token_->token_start_;
	
	while (current_token_type_ != TokenType::kTokenEOF)
	{
		ScriptASTNode *child = Parse_Statement();
		
		node->AddChild(child);
	}
	
	int token_end = current_token_->token_start_ - 1;
	
	Match(TokenType::kTokenEOF, "interpreter block");
	
	// swap in a new virtual token that encompasses all our children
	std::string token_string = script_string_.substr(token_start, token_end - token_start + 1);
	
	virtual_token = new ScriptToken(TokenType::kTokenInterpreterBlock, token_string, token_start, token_end);
	node->ReplaceTokenWithToken(virtual_token);
	
	return node;
}

ScriptASTNode *Script::Parse_CompoundStatement(void)
{
	ScriptASTNode *node = new ScriptASTNode(current_token_);
	
	int token_start = current_token_->token_start_;
	
	Match(TokenType::kTokenLBrace, "compound statement");
	
	while (current_token_type_ != TokenType::kTokenRBrace)
	{
		ScriptASTNode *child = Parse_Statement();
		
		node->AddChild(child);
	}
	
	int token_end = current_token_->token_start_;
	
	Match(TokenType::kTokenRBrace, "compound statement");
	
	// swap in a new virtual token that encompasses all our children
	std::string token_string = script_string_.substr(token_start, token_end - token_start + 1);
	
	ScriptToken *virtual_token = new ScriptToken(node->token_->token_type_, token_string, token_start, token_end);
	node->ReplaceTokenWithToken(virtual_token);
	
	return node;
}

ScriptASTNode *Script::Parse_Statement(void)
{
	if (current_token_type_ == TokenType::kTokenLBrace)
		return Parse_CompoundStatement();
	else if (current_token_type_ == TokenType::kTokenIf)
		return Parse_SelectionStatement();
	else if (current_token_type_ == TokenType::kTokenDo)
		return Parse_DoWhileStatement();
	else if (current_token_type_ == TokenType::kTokenWhile)
		return Parse_WhileStatement();
	else if (current_token_type_ == TokenType::kTokenFor)
		return Parse_ForStatement();
	else if ((current_token_type_ == TokenType::kTokenNext) || (current_token_type_ == TokenType::kTokenBreak) || (current_token_type_ == TokenType::kTokenReturn))
		return Parse_JumpStatement();
	else
		return Parse_ExprStatement();
}

ScriptASTNode *Script::Parse_ExprStatement(void)
{
	ScriptASTNode *node;
	
	if (current_token_type_ == TokenType::kTokenSemicolon)
	{
		node = new ScriptASTNode(current_token_);		// an empty statement is represented by a semicolon token
		Consume();
	}
	else
	{
		node = Parse_AssignmentExpr();
		Match(TokenType::kTokenSemicolon, "expression statement");
	}
	
	return node;
}

ScriptASTNode *Script::Parse_SelectionStatement(void)
{
	ScriptASTNode *node, *test_expr, *true_statement, *false_statement;
	
	node = new ScriptASTNode(current_token_);
	Match(TokenType::kTokenIf, "if statement");
	Match(TokenType::kTokenLParen, "if statement");
	test_expr = Parse_Expr();
	Match(TokenType::kTokenRParen, "if statement");
	true_statement = Parse_Statement();
	
	node->AddChild(test_expr);
	node->AddChild(true_statement);
	
	if (current_token_type_ == TokenType::kTokenElse)
	{
		Consume();
		false_statement = Parse_Statement();
		
		node->AddChild(false_statement);
	}
	
	return node;
}

ScriptASTNode *Script::Parse_DoWhileStatement(void)
{
	ScriptASTNode *node, *test_expr, *statement;
	
	node = new ScriptASTNode(current_token_);
	Match(TokenType::kTokenDo, "do/while statement");
	statement = Parse_Statement();
	Match(TokenType::kTokenWhile, "do/while statement");
	Match(TokenType::kTokenLParen, "do/while statement");
	test_expr = Parse_Expr();
	Match(TokenType::kTokenRParen, "do/while statement");
	Match(TokenType::kTokenSemicolon, "do/while statement");
	
	node->AddChild(statement);
	node->AddChild(test_expr);
	
	return node;
}

ScriptASTNode *Script::Parse_WhileStatement(void)
{
	ScriptASTNode *node, *test_expr, *statement;
	
	node = new ScriptASTNode(current_token_);
	Match(TokenType::kTokenWhile, "while statement");
	Match(TokenType::kTokenLParen, "while statement");
	test_expr = Parse_Expr();
	Match(TokenType::kTokenRParen, "while statement");
	statement = Parse_Statement();
	
	node->AddChild(test_expr);
	node->AddChild(statement);
	
	return node;
}

ScriptASTNode *Script::Parse_ForStatement(void)
{
	ScriptASTNode *node, *identifier, *range_expr, *statement;
	
	node = new ScriptASTNode(current_token_);
	Match(TokenType::kTokenFor, "for statement");
	Match(TokenType::kTokenLParen, "for statement");
	identifier = new ScriptASTNode(current_token_);
	Match(TokenType::kTokenIdentifier, "for statement");
	Match(TokenType::kTokenIn, "for statement");
	range_expr = Parse_Expr();
	Match(TokenType::kTokenRParen, "for statement");
	statement = Parse_Statement();
	
	node->AddChild(identifier);
	node->AddChild(range_expr);
	node->AddChild(statement);
	
	return node;
}

ScriptASTNode *Script::Parse_JumpStatement(void)
{
	ScriptASTNode *node;
	
	if ((current_token_type_ == TokenType::kTokenNext) || (current_token_type_ == TokenType::kTokenBreak))
	{
		node = new ScriptASTNode(current_token_);
		Consume();
		
		Match(TokenType::kTokenSemicolon, "next/break statement");
	}
	else if (current_token_type_ == TokenType::kTokenReturn)
	{
		node = new ScriptASTNode(current_token_);
		Consume();
		
		if (current_token_type_ == TokenType::kTokenSemicolon)
		{
			Match(TokenType::kTokenSemicolon, "return statement");
		}
		else
		{
			ScriptASTNode *value_expr = Parse_Expr();
			Match(TokenType::kTokenSemicolon, "return statement");
			
			node->AddChild(value_expr);
		}
	}
	
	return node;
}

ScriptASTNode *Script::Parse_Expr(void)
{
	return Parse_LogicalOrExpr();
}

ScriptASTNode *Script::Parse_AssignmentExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_LogicalOrExpr();
	
	if (current_token_type_ == TokenType::kTokenAssign)
	{
		if (!node)
			node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		node->AddChild(Parse_LogicalOrExpr());
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_LogicalOrExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_LogicalAndExpr();
	
	while (current_token_type_ == TokenType::kTokenOr)
	{
		if (!node)
			node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		node->AddChild(Parse_LogicalAndExpr());
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_LogicalAndExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_EqualityExpr();
	
	while (current_token_type_ == TokenType::kTokenAnd)
	{
		if (!node)
			node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		node->AddChild(Parse_EqualityExpr());
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_EqualityExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_RelationalExpr();
	
	while ((current_token_type_ == TokenType::kTokenEq) || (current_token_type_ == TokenType::kTokenNotEq))
	{
		node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		ScriptASTNode *right_expr = Parse_RelationalExpr();
		
		node->AddChild(right_expr);
		left_expr = node;
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_RelationalExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_AddExpr();
	
	while ((current_token_type_ == TokenType::kTokenLt) || (current_token_type_ == TokenType::kTokenGt) || (current_token_type_ == TokenType::kTokenLtEq) || (current_token_type_ == TokenType::kTokenGtEq))
	{
		node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		ScriptASTNode *right_expr = Parse_AddExpr();
		
		node->AddChild(right_expr);
		left_expr = node;
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_AddExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_MultExpr();
	
	while ((current_token_type_ == TokenType::kTokenPlus) || (current_token_type_ == TokenType::kTokenMinus))
	{
		node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		ScriptASTNode *right_expr = Parse_MultExpr();
		
		node->AddChild(right_expr);
		left_expr = node;
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_MultExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_SeqExpr();
	
	while ((current_token_type_ == TokenType::kTokenMult) || (current_token_type_ == TokenType::kTokenDiv) || (current_token_type_ == TokenType::kTokenMod))
	{
		node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		ScriptASTNode *right_expr = Parse_SeqExpr();
		
		node->AddChild(right_expr);
		left_expr = node;
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_SeqExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_ExpExpr();
	
	if (current_token_type_ == TokenType::kTokenColon)
	{
		if (!node)
			node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		node->AddChild(Parse_ExpExpr());
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_ExpExpr(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_UnaryExpr();
	
	while (current_token_type_ == TokenType::kTokenExp)
	{
		node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		ScriptASTNode *right_expr = Parse_UnaryExpr();
		
		node->AddChild(right_expr);
		left_expr = node;
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_UnaryExpr(void)
{
	ScriptASTNode *node;
	
	if ((current_token_type_ == TokenType::kTokenPlus) || (current_token_type_ == TokenType::kTokenMinus) || (current_token_type_ == TokenType::kTokenNot))
	{
		node = new ScriptASTNode(current_token_);
		Consume();
		
		node->AddChild(Parse_UnaryExpr());
	}
	else
	{
		node = Parse_PostfixExpr();
	}
	
	return node;
}

ScriptASTNode *Script::Parse_PostfixExpr(void)
{
	ScriptASTNode *left_expr = nullptr, *node = nullptr;
	
	left_expr = Parse_PrimaryExpr();
	
	while (true)
	{
		if (current_token_type_ == TokenType::kTokenLBracket)
		{
			node = new ScriptASTNode(current_token_, left_expr);
			Consume();
			
			ScriptASTNode *right_expr = Parse_Expr();
			node->AddChild(right_expr);
			
			Match(TokenType::kTokenRBracket, "postfix subset expression");
			left_expr = node;
		}
		else if (current_token_type_ == TokenType::kTokenLParen)
		{
			node = new ScriptASTNode(current_token_, left_expr);
			Consume();
			
			if (current_token_type_ == TokenType::kTokenRParen)
			{
				Consume();
			}
			else
			{
				ScriptASTNode *right_expr = Parse_ArgumentExprList();
				node->AddChild(right_expr);
				
				Match(TokenType::kTokenRParen, "postfix function call expression");
			}
			left_expr = node;
		}
		else if (current_token_type_ == TokenType::kTokenDot)
		{
			node = new ScriptASTNode(current_token_, left_expr);
			Consume();
			
			ScriptASTNode *identifier = new ScriptASTNode(current_token_);
			node->AddChild(identifier);
			
			Match(TokenType::kTokenIdentifier, "postfix member expression");
			left_expr = node;
		}
		else
			break;
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_PrimaryExpr(void)
{
	if ((current_token_type_ == TokenType::kTokenNumber) || (current_token_type_ == TokenType::kTokenString))
	{
		return Parse_Constant();
	}
	else if (current_token_type_ == TokenType::kTokenLParen)
	{
		Consume();
		ScriptASTNode *node = Parse_Expr();
		Match(TokenType::kTokenRParen, "primary parenthesized expression");
		
		return node;
	}
	else if (current_token_type_ == TokenType::kTokenIdentifier)
	{
		ScriptASTNode *identifier = new ScriptASTNode(current_token_);
		Match(TokenType::kTokenIdentifier, "primary identifier expression");
		
		return identifier;
	}
	else
	{
		SetErrorPositionFromCurrentToken();
		SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_PrimaryExpr" << slim_terminate();
		return nullptr;
	}
}

ScriptASTNode *Script::Parse_ArgumentExprList(void)
{
	ScriptASTNode *left_expr, *node = nullptr;
	
	left_expr = Parse_AssignmentExpr();
	
	while (current_token_type_ == TokenType::kTokenComma)
	{
		if (!node)
			node = new ScriptASTNode(current_token_, left_expr);
		Consume();
		
		node->AddChild(Parse_AssignmentExpr());
	}
	
	return (node ? node : left_expr);
}

ScriptASTNode *Script::Parse_Constant(void)
{
	ScriptASTNode *node = nullptr;
	
	if (current_token_type_ == TokenType::kTokenNumber)
	{
		node = new ScriptASTNode(current_token_);
		Match(TokenType::kTokenNumber, "number literal expression");
	}
	else if (current_token_type_ == TokenType::kTokenString)
	{
		node = new ScriptASTNode(current_token_);
		Match(TokenType::kTokenString, "string literal expression");
	}
	else
	{
		SetErrorPositionFromCurrentToken();
		SLIM_TERMINATION << "ERROR (Parse): unexpected token " << *current_token_ << " in Parse_Constant" << slim_terminate();
		return nullptr;
	}
	
	return node;
}

void Script::ParseSLiMFileToAST(void)
{
	// delete the existing AST
	delete parse_root_;
	parse_root_ = nullptr;
	
	// set up parse state
	parse_index_ = 0;
	current_token_ = token_stream_.at(parse_index_);		// should always have at least an EOF
	current_token_type_ = current_token_->token_type_;
	
	// parse a new AST from our start token
	ScriptASTNode *tree = Parse_SLiMFile();
	
	parse_root_ = tree;
	
	// if logging of the AST is requested, do that; always to cout, not to SLIM_OUTSTREAM
	if (gSLiMScriptLogAST)
	{
		std::cout << "AST : \n";
		this->PrintAST(std::cout);
	}
}

void Script::ParseInterpreterBlockToAST(void)
{
	// delete the existing AST
	delete parse_root_;
	parse_root_ = nullptr;
	
	// set up parse state
	parse_index_ = 0;
	current_token_ = token_stream_.at(parse_index_);		// should always have at least an EOF
	current_token_type_ = current_token_->token_type_;
	
	// parse a new AST from our start token
	ScriptASTNode *tree = Parse_InterpreterBlock();
	
	parse_root_ = tree;
	
	// if logging of the AST is requested, do that; always to cout, not to SLIM_OUTSTREAM
	if (gSLiMScriptLogAST)
	{
		std::cout << "AST : \n";
		this->PrintAST(std::cout);
	}
}

void Script::PrintTokens(ostream &p_outstream) const
{
	if (token_stream_.size())
	{
		for (auto token : token_stream_)
			p_outstream << *token << " ";
		
		p_outstream << endl;
	}
}

void Script::PrintAST(ostream &p_outstream) const
{
	if (parse_root_)
	{
		parse_root_->PrintTreeWithIndent(p_outstream, 0);
		
		p_outstream << endl;
	}
}





























































