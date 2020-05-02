//
//  QtSLiMEidosPrettyprinter.cpp
//  SLiM
//
//  Created by Ben Haller on 8/1/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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


#include "QtSLiMEidosPrettyprinter.h"
#include <eidos_script.h>

#include <QString>
#include <QChar>


static int Eidos_indentForStack(std::vector<const EidosToken *> &indentStack, bool startingNewStatement, EidosTokenType nextTokenType)
{
    // Count the number of indents represented by the indent stack.  When a control-flow keyword is
	// followed by a left brace, the indent stack has two items on it, but we want to only indent
	// one level.
	int indent = 0;
	bool previousIndentStackItemWasControlFlow = false;
	
	for (size_t stackIndex = 0; stackIndex < indentStack.size(); ++stackIndex)
	{
		EidosTokenType stackTokenType = indentStack[stackIndex]->token_type_;
		
		// skip over ternary conditionals; they do not generate indent, but are on the stack so we can match elses
		if (stackTokenType == EidosTokenType::kTokenConditional)
			continue;
		
		if (previousIndentStackItemWasControlFlow && (stackTokenType == EidosTokenType::kTokenLBrace))
			;
		else
			++indent;
		
		previousIndentStackItemWasControlFlow = !(stackTokenType == EidosTokenType::kTokenLBrace);
	}
	
	bool lastIndentIsControlFlow = previousIndentStackItemWasControlFlow;
	
	// Indent when continuing a statement, but not after a control-flow token.  The idea here is that
	// if you have a structure like:
	//
	//	if (x)
	//		if (y)
	//			<statement>;
	//
	// the indent stack will already dictate that <statement> is indented twice; it does not need to
	// receive the !startingNewStatement extra indent level that we normally add to cause continuing
	// statements to be indented like:
	//
	//	x = a + b + c +
	//		d + e + f;
	//
	if (!startingNewStatement && !lastIndentIsControlFlow)
		indent++;
	
	// If the next token is a left brace, outdent one level, conventionally.  This reflects usage like:
	//
	//	if (x)
	//		y;
	//
	//	if (x)
	//	{
	//		y;
	//	}
	//
	// This applies only if the last element on the indent stack is a control-flow indent, not a {.
	// This is the same rule we used when counting indentStack, but applied to nextTokenType.  We also
	// outdent when we see a left brace if we are mid-statement; this covers SLiM callback syntax.
	//
	if ((lastIndentIsControlFlow || !startingNewStatement) && (nextTokenType == EidosTokenType::kTokenLBrace))
		indent--;
	
	// For similar reasons, if the next token is a right brace, always outdent one level
	if (nextTokenType == EidosTokenType::kTokenRBrace)
		indent--;
	
	return indent;
}

bool Eidos_prettyprintTokensFromScript(const std::vector<EidosToken> &tokens, EidosScript &tokenScript, std::string &pretty)
{
    // We keep a stack of indent-generating tokens: { if else do while for.  The purpose of this is
	// to be able to tell what indent level we're at, and how it changes with a ; or a } token.
	std::vector<const EidosToken *> indentStack;
	bool startingNewStatement = true;
	size_t tokenCount = tokens.size();
	
	for (size_t tokenIndex = 0; tokenIndex < tokenCount; ++tokenIndex)
	{
		const EidosToken &token = tokens[tokenIndex];
        std::string tokenString(token.token_string_);
		EidosTokenType nextTokenPeek = (tokenIndex + 1 < tokenCount ? tokens[tokenIndex+1].token_type_ : EidosTokenType::kTokenEOF);
		
		// Find the next non-whitespace, non-comment token for lookahead
		size_t nextSignificantTokenPeekIndex = tokenIndex + 1;
		EidosTokenType nextSignificantTokenPeek = EidosTokenType::kTokenEOF;
		
		while (nextSignificantTokenPeekIndex < tokenCount)
		{
			EidosTokenType peek = tokens[nextSignificantTokenPeekIndex].token_type_;
			
			if ((peek != EidosTokenType::kTokenWhitespace) && (peek != EidosTokenType::kTokenComment) && (peek != EidosTokenType::kTokenCommentLong))
			{
				nextSignificantTokenPeek = peek;
				break;
			}
			
			nextSignificantTokenPeekIndex++;
		}
		
		switch (token.token_type_)
		{
				// These token types are not used in the AST and should not be present
			case EidosTokenType::kTokenNone:
			case EidosTokenType::kTokenBad:
				return false;
				
				// These are virtual tokens that can be ignored
			case EidosTokenType::kTokenEOF:
			case EidosTokenType::kTokenInterpreterBlock:
			case EidosTokenType::kTokenContextFile:
			case EidosTokenType::kTokenContextEidosBlock:
			case EidosTokenType::kFirstIdentifierLikeToken:
				break;
				
				// This is where the rubber meets the road; prettyprinting is all about altering whitespace stretches.
				// We don't want to alter the user's newline decisions, so we count the number of newlines in this
				// whitespace stretch and always emit the same number.  If there are no newlines, we're in whitespace
				// inside a given line, with tokens on both sides; for the time being we do not alter those at all.
				// If there are newlines, though, then each newline is changed to be followed by the appropriate number
				// of tabs as indentation.  The indent depends upon the indent stack and some other state about the
				// context we are in.
			case EidosTokenType::kTokenWhitespace:
			{
                // We use QString to get intelligent treatment of Unicode and UTF-8; std::string is just too dumb.
                // In SLiMGui this is done with NSString.  We want to count newlines in a mac/unix/windows agnostic way.
                QString q_tokenString = QString::fromStdString(tokenString);
                int newlineCount = 0;
                bool prevWasCR = false, prevWasLF = false;
                
                for (QChar qch : q_tokenString)
                {
                    if (qch == QChar::LineFeed)
                    {
                        if (prevWasCR) { prevWasCR = false; continue; }     // CR-LF counts for one
                        newlineCount++;
                        prevWasLF = true;
                    }
                    else if (qch == QChar::CarriageReturn)
                    {
                        if (prevWasLF) { prevWasLF = false; continue; }     // LF-CR counts for one
                        newlineCount++;
                        prevWasCR = true;
                    }
                    else if (qch == QChar::ParagraphSeparator)
                    {
                        newlineCount++;
                        prevWasCR = prevWasLF = false;
                    }
                }
                
				if (newlineCount <= 0)
				{
					// Normally, whitespace tokens that do not contain a newline occur inside a line, and should be preserved.
					// A whitespace token that indents the start of a line normally started on the previous line and contains
					// a newline.  However, this is not the case at the very beginning of a script; the first token is special.
					if (tokenIndex > 0)
                        pretty.append(tokenString);
				}
				else
				{
                    int indent = Eidos_indentForStack(indentStack, startingNewStatement, nextTokenPeek);
					
					for (int lineCounter = 0; lineCounter < newlineCount; ++lineCounter)
					{
                        pretty.append("\n");
						
						for (int tabCounter = 0; tabCounter < indent; ++tabCounter)
                            pretty.append("\t");
					}
				}
				
				break;
			}
				
				// We have ended a statement, so we reset our indent levels
			case EidosTokenType::kTokenSemicolon:
			{
				// Pop indent-generating tokens that have expired with the end of this statement; a semicolon terminates
				// a whole nested series of if else do while for, but does not terminate an enclosing { block.  Also,
				// if there are nested if statements, a semicolon terminates only the first one if the next token is an else.
				while (indentStack.size() > 0) {
					const EidosToken *topIndentToken = indentStack.back();
					
					if (topIndentToken->token_type_ == EidosTokenType::kTokenLBrace)
						break;
					if ((topIndentToken->token_type_ == EidosTokenType::kTokenIf) && (nextSignificantTokenPeek == EidosTokenType::kTokenElse))
					{
						indentStack.pop_back();
						break;
					}
					
					indentStack.pop_back();
				}
				
                pretty.append(tokenString);
				break;
			}
				
				// Track braces
			case EidosTokenType::kTokenLBrace:
			{
				indentStack.push_back(&token);
                pretty.append(tokenString);
				break;
			}
			case EidosTokenType::kTokenRBrace:
			{
				// First pop the matching left brace
				if ((indentStack.size() == 0) || (indentStack.back()->token_type_ != EidosTokenType::kTokenLBrace))
				{
					// All other indent-producing tokens should already have been balanced; Eidos has no implicit termination of statements
					//NSLog(@"Unbalanced '}' token in prettyprinting!");
					return false;
				}
				indentStack.pop_back();
				
				// Then pop indent-generating tokens above the left brace that have expired with the end of this statement
				while (indentStack.size() > 0) {
					const EidosToken *topIndentToken = indentStack.back();
					
					if (topIndentToken->token_type_ == EidosTokenType::kTokenLBrace)
						break;
					
					indentStack.pop_back();
				}
				
                pretty.append(tokenString);
				break;
			}
				
				// Control-flow keywords influence our indent level; this might look like the normal statement inner indent,
				// but it is not, as can be seen when these control-flow keywords are nested like 'if (x) if (y) <statement>'.
				// When an if follows an else, the else is removed by the if, since we don't want two indents; else-if is one indent.
			case EidosTokenType::kTokenIf:
			{
				if (indentStack.size())
				{
					EidosTokenType top_token_type = indentStack.back()->token_type_;
					
					if (top_token_type == EidosTokenType::kTokenElse)
						indentStack.pop_back();
				}
				
				indentStack.push_back(&token);
                pretty.append(tokenString);
				break;
			}
				
			case EidosTokenType::kTokenDo:
			case EidosTokenType::kTokenWhile:
			case EidosTokenType::kTokenFor:
			case EidosTokenType::kTokenConditional:		// note this does not generate indent, but is put on the stack
			{
				indentStack.push_back(&token);
                pretty.append(tokenString);
				break;
			}
				
				// else can be paired with if or ?.  In the former case, the if will be off the stack by the time the else
				// is encountered, and we put the else on to give us an equivalent indent.  In the latter case, we consider
				// the expressions within the ternary conditional to be statement-level; we don't indent, and we don't push
				// an else on the stack here, but we remove the conditional that we are completing.
			case EidosTokenType::kTokenElse:
			{
				if (indentStack.size())
				{
					EidosTokenType top_token_type = indentStack.back()->token_type_;
					
					if (top_token_type == EidosTokenType::kTokenConditional)
					{
						indentStack.pop_back();
                        pretty.append(tokenString);
						break;
					}
				}
				
				indentStack.push_back(&token);
                pretty.append(tokenString);
				break;
			}
				
				// Comments are preserved verbatim
			case EidosTokenType::kTokenComment:
			case EidosTokenType::kTokenCommentLong:
                pretty.append(tokenString);
				break;
				
				// Tokens for operators are emitted verbatim
			case EidosTokenType::kTokenColon:
			case EidosTokenType::kTokenComma:
			case EidosTokenType::kTokenDot:
			case EidosTokenType::kTokenPlus:
			case EidosTokenType::kTokenMinus:
			case EidosTokenType::kTokenMod:
			case EidosTokenType::kTokenMult:
			case EidosTokenType::kTokenExp:
			case EidosTokenType::kTokenAnd:
			case EidosTokenType::kTokenOr:
			case EidosTokenType::kTokenDiv:
			case EidosTokenType::kTokenAssign:
			case EidosTokenType::kTokenEq:
			case EidosTokenType::kTokenLt:
			case EidosTokenType::kTokenLtEq:
			case EidosTokenType::kTokenGt:
			case EidosTokenType::kTokenGtEq:
			case EidosTokenType::kTokenNot:
			case EidosTokenType::kTokenNotEq:
                pretty.append(tokenString);
				break;
				
			case EidosTokenType::kTokenSingleton:
                pretty.append(tokenString);
				break;
				
				// Nesting levels of parens and brackets are not tracked at the moment
			case EidosTokenType::kTokenLParen:
			case EidosTokenType::kTokenRParen:
			case EidosTokenType::kTokenLBracket:
			case EidosTokenType::kTokenRBracket:
                pretty.append(tokenString);
				break;
				
				// Numbers and identifiers are emitted verbatim
			case EidosTokenType::kTokenNumber:
			case EidosTokenType::kTokenIdentifier:
                pretty.append(tokenString);
				break;
				
				// Strings are emitted verbatim, but their original string needs to be reconstructed;
				// token_string_ has the outer quotes removed and escape sequences resolved
			case EidosTokenType::kTokenString:
                pretty.append(tokenScript.String().substr(static_cast<size_t>(token.token_start_), static_cast<size_t>(token.token_end_ - token.token_start_ + 1)));
				break;
				
				// These keywords have no effect on indent level
			case EidosTokenType::kTokenIn:
			case EidosTokenType::kTokenNext:
			case EidosTokenType::kTokenBreak:
			case EidosTokenType::kTokenReturn:
			case EidosTokenType::kTokenFunction:
                pretty.append(tokenString);
				break;
		}
		
		// Now that we're done processing that token, update startingNewStatement to reflect whether we are within
		// a statement, of which we have seen at least one token, or starting a new statement.  Nonsignificant
		// tokens (whitespace and comments) do not alter the state of startingNewStatement.
		if ((token.token_type_ != EidosTokenType::kTokenWhitespace) && (token.token_type_ != EidosTokenType::kTokenComment) && (token.token_type_ != EidosTokenType::kTokenCommentLong))
			startingNewStatement = ((token.token_type_ == EidosTokenType::kTokenSemicolon) ||
									(token.token_type_ == EidosTokenType::kTokenLBrace) ||
									(token.token_type_ == EidosTokenType::kTokenRBrace));
	}
	
	return true;
}


































