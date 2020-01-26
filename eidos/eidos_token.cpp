//
//  eidos_token.cpp
//  Eidos
//
//  Created by Ben Haller on 7/27/15.
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


#include "eidos_token.h"


std::ostream &operator<<(std::ostream &p_outstream, const EidosTokenType p_token_type)
{
	switch (p_token_type)
	{
		case EidosTokenType::kTokenNone:				p_outstream << "NO_TOKEN";		break;
		case EidosTokenType::kTokenBad:					p_outstream << "BAD_TOKEN";		break;
		case EidosTokenType::kTokenEOF:					p_outstream << "EOF";			break;
		case EidosTokenType::kTokenWhitespace:			p_outstream << "WS";			break;
		case EidosTokenType::kTokenSemicolon:			p_outstream << ";";				break;
		case EidosTokenType::kTokenColon:				p_outstream << ":";				break;
		case EidosTokenType::kTokenComma:				p_outstream << ",";				break;
		case EidosTokenType::kTokenLBrace:				p_outstream << "{";				break;
		case EidosTokenType::kTokenRBrace:				p_outstream << "}";				break;
		case EidosTokenType::kTokenLParen:				p_outstream << "(";				break;
		case EidosTokenType::kTokenRParen:				p_outstream << ")";				break;
		case EidosTokenType::kTokenLBracket:			p_outstream << "[";				break;
		case EidosTokenType::kTokenRBracket:			p_outstream << "]";				break;
		case EidosTokenType::kTokenDot:					p_outstream << ".";				break;
		case EidosTokenType::kTokenPlus:				p_outstream << "+";				break;
		case EidosTokenType::kTokenMinus:				p_outstream << "-";				break;
		case EidosTokenType::kTokenMod:					p_outstream << "%";				break;
		case EidosTokenType::kTokenMult:				p_outstream << "*";				break;
		case EidosTokenType::kTokenExp:					p_outstream << "^";				break;
		case EidosTokenType::kTokenAnd:					p_outstream << "&";				break;
		case EidosTokenType::kTokenOr:					p_outstream << "|";				break;
		case EidosTokenType::kTokenDiv:					p_outstream << "/";				break;
		case EidosTokenType::kTokenConditional:			p_outstream << "?";				break;
		case EidosTokenType::kTokenComment:				p_outstream << "COMMENT";		break;
		case EidosTokenType::kTokenCommentLong:			p_outstream << "COMMENT_LONG";	break;
		case EidosTokenType::kTokenAssign:				p_outstream << "=";				break;
		case EidosTokenType::kTokenEq:					p_outstream << "==";			break;
		case EidosTokenType::kTokenLt:					p_outstream << "<";				break;
		case EidosTokenType::kTokenLtEq:				p_outstream << "<=";			break;
		case EidosTokenType::kTokenGt:					p_outstream << ">";				break;
		case EidosTokenType::kTokenGtEq:				p_outstream << ">=";			break;
		case EidosTokenType::kTokenNot:					p_outstream << "!";				break;
		case EidosTokenType::kTokenNotEq:				p_outstream << "!=";			break;
		case EidosTokenType::kTokenSingleton:			p_outstream << "$";				break;
		case EidosTokenType::kTokenNumber:				p_outstream << "NUMBER";		break;
		case EidosTokenType::kTokenString:				p_outstream << "STRING";		break;
		case EidosTokenType::kTokenIdentifier:			p_outstream << "IDENTIFIER";	break;
		case EidosTokenType::kTokenIf:					p_outstream << gEidosStr_if;		break;
		case EidosTokenType::kTokenElse:				p_outstream << gEidosStr_else;		break;
		case EidosTokenType::kTokenDo:					p_outstream << gEidosStr_do;		break;
		case EidosTokenType::kTokenWhile:				p_outstream << gEidosStr_while;		break;
		case EidosTokenType::kTokenFor:					p_outstream << gEidosStr_for;		break;
		case EidosTokenType::kTokenIn:					p_outstream << gEidosStr_in;		break;
		case EidosTokenType::kTokenNext:				p_outstream << gEidosStr_next;		break;
		case EidosTokenType::kTokenBreak:				p_outstream << gEidosStr_break;		break;
		case EidosTokenType::kTokenReturn:				p_outstream << gEidosStr_return;	break;
		case EidosTokenType::kTokenFunction:			p_outstream << gEidosStr_function;	break;
			
		case EidosTokenType::kTokenInterpreterBlock:	p_outstream << "$>";			break;
		case EidosTokenType::kTokenContextFile:			p_outstream << "###";			break;
		case EidosTokenType::kTokenContextEidosBlock:	p_outstream << "#>";			break;
		case EidosTokenType::kFirstIdentifierLikeToken:	p_outstream << "???";			break;
	}
	
	return p_outstream;
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosToken &p_token)
{
	// print strings, identifiers, numbers, and keywords with identifying marks; apart from that, print tokens as is
	if (p_token.token_type_ == EidosTokenType::kTokenString)
		p_outstream << "\"" << p_token.token_string_ << "\"";
	else if (p_token.token_type_ == EidosTokenType::kTokenIdentifier)
		p_outstream << "@" << p_token.token_string_;
	else if (p_token.token_type_ == EidosTokenType::kTokenNumber)
		p_outstream << "#" << p_token.token_string_;
	else if (p_token.token_type_ > EidosTokenType::kFirstIdentifierLikeToken)
		p_outstream << "<" << p_token.token_string_ << ">";	// <> delimiters help distinguish keywords from identifiers
	else
		p_outstream << p_token.token_type_;
	
	return p_outstream;
}




















































