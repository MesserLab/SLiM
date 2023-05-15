//
//  eidos_token.h
//  Eidos
//
//  Created by Ben Haller on 7/27/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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


#ifndef __Eidos__eidos_token__
#define __Eidos__eidos_token__

#include <stdio.h>

#include <string>
#include <ostream>

#include "eidos_globals.h"


// An enumeration for all token types, whether real or virtual
enum class EidosTokenType : int16_t {
	kTokenNone = 0,		//			no token; this type should not be in the final token stream
	kTokenBad,			//			bad token; produced if Tokenize() is instructed not to raise
	kTokenEOF,			//			end of file; an EOF token is produced explicitly
	kTokenWhitespace,	//			spaces, tabs, newlines
	
	kTokenSemicolon,	// ;		statement terminator
	kTokenColon,		// :		range operator, as in R
	kTokenComma,		// ,		presently used for separating function parameters only
	kTokenLBrace,		// {		block delimiter
	kTokenRBrace,		// }		block delimiter
	kTokenLParen,		// (		subexpression delimiter (also used in type specifiers)
	kTokenRParen,		// )		subexpression delimiter
	kTokenLBracket,		// [		subset operator
	kTokenRBracket,		// ]		subset operator
	kTokenDot,			// .		member operator
	kTokenPlus,			// +		addition operator (also used in type specifiers)
	kTokenMinus,		// -		subtraction operator (unary or binary)
	kTokenMod,			// %		modulo operator
	kTokenMult,			// *		multiplication operator (also used in type specifiers)
	kTokenExp,			// ^		exponentiation operator
	kTokenAnd,			// &		boolean AND
	kTokenOr,			// |		boolean OR
	kTokenDiv,			// /		division operator
	kTokenConditional,	// ?		ternary conditional, with 'else'
	
	kTokenComment,		// //		comment
	kTokenCommentLong,	// /*		comment
	kTokenAssign,		// =		assignment
	kTokenAssign_R,		// <-		assignment in the R style, which is an illegal token in Eidos to avoid errors
	kTokenEq,			// ==		equality test
	kTokenLt,			// <		less than test (also used in type specifiers)
	kTokenLtEq,			// <=		less than or equals test
	kTokenGt,			// >		greater than test (also used in type specifiers)
	kTokenGtEq,			// >=		greater than or equals test
	kTokenNot,			// !		boolean NOT
	kTokenNotEq,		// !=		not equals test
	
	kTokenSingleton,	// $		used to indicate a singleton type in type-specifiers
	
	kTokenNumber,		//			there is a single numeric token type for both ints and floats, for now at least
	kTokenString,		//			string literals are bounded by double quotes only, and recognize some escapes
	kTokenIdentifier,	//			all valid identifiers that are not keywords or operators
	
	// ----- VIRTUAL TOKENS; THESE WILL HAVE A STRING OF "" AND A LENGTH OF 0
	
	kTokenInterpreterBlock,			// a block of statements executed as a unit in the interpreter
	
	// these virtual token types are not used by Eidos itself; they are provided as a convenience for
	// Contexts that embed Eidos within larger script files in a Context-defined format
	
	kTokenContextFile,				// an Eidos-based input file containing zero or more Eidos blocks in a Context-defined format
	kTokenContextEidosBlock,		// an Eidos-based script block with additional tokens in a Context-defined format
	
	// ----- ALL TOKENS AFTER THIS POINT SHOULD BE KEYWORDS MATCHED BY kTokenIdentifier
	
	kFirstIdentifierLikeToken,
	kTokenIf,			// if		conditional
	kTokenElse,			// else		conditional (and ternary conditional)
	kTokenDo,			// do		loop while condition true
	kTokenWhile,		// while	loop while condition true
	kTokenFor,			// for		loop over set
	kTokenIn,			// in		loop over set
	kTokenNext,			// next		loop jump to end
	kTokenBreak,		// break	loop jump to completion
	kTokenReturn,		// return	return a value from the enclosing block
	kTokenFunction,		// function	define a user-defined function
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosTokenType p_token_type);


// A class representing a single token read from a script string
class EidosToken
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
public:
	
	const std::string token_string_;			// extracted string object for the token
	const EidosTokenType token_type_;			// the type of the token; one of the enumeration above
	const int32_t token_start_;					// character position within script_string_
	const int32_t token_end_;					// character position within script_string_
	
	// These are only used in the GUI environment, where we need token positions in terms of NSString's character indices.
	// They are calculated in all compile environments, though, since the overhead is small.
	const int32_t token_UTF16_start_;			// the same as token_start_ but in UTF-16 code units, as NSString uses
	const int32_t token_UTF16_end_;				// the same as token_end_ but in UTF-16 code units, as NSString uses
	
	// This is presently used only for debug points in SLiMgui, but may prove useful in some other contexts.
	// It keeps the line position of the token within the full user script string, or -1 if this token does not live
	// in the user's script string.  Note that this may not correspond to lines in the owning EidosScript, if that
	// EidosScript was constructed with non-zero base line number (i.e., represents a snippet from the full script).
	const int32_t token_line_;
	
	EidosToken(const EidosToken&) = default;				// default copy-construction, for std::vector
	EidosToken& operator=(const EidosToken&) = delete;		// no copying
	EidosToken(void) = delete;								// no null construction
	
	inline EidosToken(EidosTokenType p_token_type, const std::string &p_token_string, int32_t p_token_start, int32_t p_token_end, int32_t p_token_UTF16_start, int32_t p_token_UTF16_end, int32_t p_token_line) :
	token_string_(p_token_string), token_type_(p_token_type), token_start_(p_token_start), token_end_(p_token_end), token_UTF16_start_(p_token_UTF16_start), token_UTF16_end_(p_token_UTF16_end), token_line_(p_token_line)
	{
	}
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosToken &p_token);


// Setting the error position; call just before you throw, or better, pass the token to EidosTerminate()
inline __attribute__((always_inline)) EidosErrorPosition PushErrorPositionFromToken(const EidosToken *p_naughty_token_)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("PushErrorPositionFromToken(): gEidosErrorContext change");
	
	EidosErrorPosition old_position = gEidosErrorContext.errorPosition;
	
	gEidosErrorContext.errorPosition.characterStartOfError = p_naughty_token_->token_start_;
	gEidosErrorContext.errorPosition.characterEndOfError = p_naughty_token_->token_end_;
	gEidosErrorContext.errorPosition.characterStartOfErrorUTF16 = p_naughty_token_->token_UTF16_start_;
	gEidosErrorContext.errorPosition.characterEndOfErrorUTF16 = p_naughty_token_->token_UTF16_end_;
	
	return old_position;
}


#endif /* defined(__Eidos__eidos_token__) */





























































