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
#include <QDebug>


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

static inline const EidosToken &NextSignificantToken(const std::vector<EidosToken> &tokens, size_t tokenIndex)
{
    // This scans forward from tokenIndex (not including tokenIndex itself) and returns the
    // next token that is not a whitespace, comment, or other non-significant token.
    do {
        if (tokenIndex == tokens.size() - 1)
            return tokens[tokenIndex];
        
        tokenIndex++;
        
        EidosTokenType tokenType = tokens[tokenIndex].token_type_;
        
        if ((tokenType != EidosTokenType::kTokenWhitespace) &&
                (tokenType != EidosTokenType::kTokenComment) &&
                (tokenType != EidosTokenType::kTokenCommentLong))
            return tokens[tokenIndex];
    } while (true);
}

static inline const EidosToken &PreviousSignificantToken(const std::vector<EidosToken> &tokens, size_t tokenIndex)
{
    // This scans backward from tokenIndex (not including tokenIndex itself) and returns the
    // previous token that is not a whitespace, comment, or other non-significant token.
    do {
        if (tokenIndex == 0)
            return tokens[tokenIndex];
        
        tokenIndex--;
        
        EidosTokenType tokenType = tokens[tokenIndex].token_type_;
        
        if ((tokenType != EidosTokenType::kTokenWhitespace) &&
                (tokenType != EidosTokenType::kTokenComment) &&
                (tokenType != EidosTokenType::kTokenCommentLong))
            return tokens[tokenIndex];
    } while (true);
}

static inline void EmitWhitespace(bool &forceSpace, int &forceNewlineCount, std::string &pretty)
{
    // This emits space before the next token.  It should be called immediately before the next token is emitted, so that
    // other considerations can influence the nature of the whitespace before it is actually appended to the string.
    if (pretty.length() > 0)
    {
        if (forceNewlineCount > 0)
        {
            for (int i = 0; i < forceNewlineCount; ++i)
                pretty.append("\n");
        }
        else if (forceSpace)
            pretty.append(" ");
    }
    
    forceSpace = false;
    forceNewlineCount = 0;
}

bool Eidos_reformatTokensFromScript(const std::vector<EidosToken> &tokens, EidosScript &tokenScript, std::string &pretty)
{
    // Completely reformat the script string, changing newlines and spaces as well as line indents.  This is different enough
    // in its logic that it doesn't seem to make sense for it to share code with Eidos_prettyprintTokensFromScript(); it would
    // just be a mess.  However, there is certainly a lot of shared logic, too.  Note that we do not deal with indentation at
    // all here.  Instead, we just call Eidos_prettyprintTokensFromScript() at the end to prettyprint the reformatted code.
    size_t tokenCount = tokens.size();
    int parenNestCount = 0;
    int braceNestCount = 0;
    bool forceNewlineAfterParenBalance = false;
    bool resolveWhileSemanticsAfterParenBalance = false;
    int insideTernaryConditionalCount = 0;
    bool lastTokenContainedNewline = true;
    bool lastTokenSuppressesCommentSpacing = true;
    int functionDeclarationCountdown = 0;
    bool forceSpace = false;
    int forceNewlineCount = 0;
	
	for (size_t tokenIndex = 0; tokenIndex < tokenCount; ++tokenIndex)
	{
		const EidosToken &token = tokens[tokenIndex];
        const std::string &tokenString = token.token_string_;
        EidosTokenType tokenType = token.token_type_;
        bool nextLastTokenSuppressesCommentSpacing = false;
        
        switch (tokenType)
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
            
            // Whitespace is completely ignored; we do our own whitespace.  We do look to see whether a newline is
            // present, though, so that we can keep comments on the same line as code when that situation exists
        case EidosTokenType::kTokenWhitespace:
            lastTokenContainedNewline = (tokenString.find_first_of("\n\r\x0C") != std::string::npos);
            break;
            
            // Comments are copied verbatim, and always get a newline after them; whether they get a newline before depends
        case EidosTokenType::kTokenComment:
        case EidosTokenType::kTokenCommentLong:
        {
            int postCommentNewlineCount = 1;
            
            if (lastTokenContainedNewline)
            {
                // we like to have a blank line before standalone comments, unless they follow a brace or a standalone comment
                forceNewlineCount = std::max(forceNewlineCount, lastTokenSuppressesCommentSpacing ? 1 : 2);
                nextLastTokenSuppressesCommentSpacing = true;
            }
            else
            {
                // same-line comments don't get a preceding newline, but if that means we're suppressing newlines,
                // make up for it after ourselves; we're just a tack-on on top of whatever was already happening
                forceSpace = true;
                postCommentNewlineCount = forceNewlineCount;
                forceNewlineCount = 0;
            }
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            if ((tokenType == EidosTokenType::kTokenComment) || lastTokenContainedNewline)
                forceNewlineCount = postCommentNewlineCount;
            else
                forceSpace = true;
            break;
        }
            
            // These tokens get no space before them, even if requested by the previous token; and they are followed by a newline
        case EidosTokenType::kTokenSemicolon:
            forceSpace = false;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceNewlineCount = 1;
            break;
            
            // This post-increments the indent level, and is always followed by a newline
        case EidosTokenType::kTokenLBrace:
            forceNewlineCount = 1;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceNewlineCount = 1;
            braceNestCount++;
            nextLastTokenSuppressesCommentSpacing = true;
            break;
            
            // This pre-decrements the indent level, and is always followed by a newline – two newlines at the topmost level
        case EidosTokenType::kTokenRBrace:
            forceNewlineCount = 1;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            braceNestCount--;
            forceNewlineCount = 1;
            if ((braceNestCount == 0) && (parenNestCount == 0))
                forceNewlineCount = 2;            
            break;
            
            // Plus and minus can be binary or unary; emit like kTokenMult if binary, like kTokenNot if unary
        case EidosTokenType::kTokenPlus:
        case EidosTokenType::kTokenMinus:
        {
            bool isBinary = false;
            const EidosToken &previous_token = PreviousSignificantToken(tokens, tokenIndex);
            EidosTokenType prev_type = previous_token.token_type_;
            
            if ((prev_type == EidosTokenType::kTokenNumber) ||
                    (prev_type == EidosTokenType::kTokenString) ||
                    (prev_type == EidosTokenType::kTokenIdentifier) ||
                    (prev_type == EidosTokenType::kTokenRParen) ||
                    (prev_type == EidosTokenType::kTokenRBracket))
                isBinary = true;
            
            if (isBinary)
                forceSpace = true;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            if (isBinary)
                forceSpace = true;
            break;
        }
            
            // These tokens should be emitted verbatim, surrounded by single spaces
        case EidosTokenType::kTokenMod:
        case EidosTokenType::kTokenMult:
        case EidosTokenType::kTokenAnd:
        case EidosTokenType::kTokenOr:
        case EidosTokenType::kTokenDiv:
        case EidosTokenType::kTokenConditional:
        case EidosTokenType::kTokenEq:
        case EidosTokenType::kTokenLt:
        case EidosTokenType::kTokenLtEq:
        case EidosTokenType::kTokenGt:
        case EidosTokenType::kTokenGtEq:
        case EidosTokenType::kTokenNotEq:
        case EidosTokenType::kTokenIn:
            if ((functionDeclarationCountdown > 0) &&
                    ((tokenType == EidosTokenType::kTokenLt) || (tokenType == EidosTokenType::kTokenGt)))
            {
                // special treatment for "o<object-type>" syntax in function declarations
                forceSpace = false;
                EmitWhitespace(forceSpace, forceNewlineCount, pretty);
                pretty.append(tokenString);
                if (tokenType == EidosTokenType::kTokenGt)
                    forceSpace = true;
            }
            else
            {
                forceSpace = true;
                EmitWhitespace(forceSpace, forceNewlineCount, pretty);
                pretty.append(tokenString);
                forceSpace = true;
                if (tokenType == EidosTokenType::kTokenConditional)
                    insideTernaryConditionalCount++;
            }
            break;
            
            // This token gets spaces around it if it's not inside parentheses, like x = y;, but no spaces inside parens, like foo(x=y);
        case EidosTokenType::kTokenAssign:
            if (parenNestCount == 0)
                forceSpace = true;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            if (parenNestCount == 0)
                forceSpace = true;
            break;
            
            // These tokens should get a space before them, but none after them
            pretty.append(tokenString);
            break;
            
            // These tokens should get a space after them, but should force there to be none before them
        case EidosTokenType::kTokenComma:
        case EidosTokenType::kTokenSingleton:
        case EidosTokenType::kTokenReturn:
            forceSpace = false;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceSpace = true;
            break;
            
            // These tokens should not get spaces on either side of them (unless the adjecent token demands it)
        case EidosTokenType::kTokenColon:
        case EidosTokenType::kTokenLBracket:
        case EidosTokenType::kTokenRBracket:
        case EidosTokenType::kTokenDot:
        case EidosTokenType::kTokenExp:
        case EidosTokenType::kTokenNot:
        case EidosTokenType::kTokenNext:
        case EidosTokenType::kTokenBreak:
        case EidosTokenType::kTokenNumber:
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            break;
            
            // Identifiers have special spacing at the top level, because constructs like "s1 1000 early()" don't follow normal rules
        case EidosTokenType::kTokenIdentifier:
            if ((parenNestCount == 0) && (braceNestCount == 0) && (functionDeclarationCountdown == 0))
                forceSpace = true;
            if (PreviousSignificantToken(tokens, tokenIndex).token_type_ == EidosTokenType::kTokenIdentifier)
                forceSpace = true;  // always force a space between two adjacent identifiers, like [lif foo=T]
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            if ((parenNestCount == 0) && (braceNestCount == 0) && (NextSignificantToken(tokens, tokenIndex).token_type_ == EidosTokenType::kTokenNumber))
                forceSpace = true;
            break;
            
            // Same as the above category, but string tokens have to be emitted in a special way
        case EidosTokenType::kTokenString:
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenScript.String().substr(static_cast<size_t>(token.token_start_), static_cast<size_t>(token.token_end_ - token.token_start_ + 1)));
            break;
            
            // Left parentheses keep track of their nesting state
        case EidosTokenType::kTokenLParen:
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            parenNestCount++;
            break;
            
            // Right parentheses keep track of their nesting state, and can do some special actions if they balance out
        case EidosTokenType::kTokenRParen:
            forceSpace = false;     // never a space before a right paren
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            if (--parenNestCount == 0)
            {
                if (forceNewlineAfterParenBalance)
                {
                    forceNewlineAfterParenBalance = false;
                    forceNewlineCount = 1;
                }
                if (resolveWhileSemanticsAfterParenBalance)
                {
                    resolveWhileSemanticsAfterParenBalance = false;
                    
                    // We rely here on the fact that the script parses without errors
                    const EidosToken &nextSigToken = NextSignificantToken(tokens, tokenIndex);
                    
                    if (nextSigToken.token_type_ != EidosTokenType::kTokenSemicolon)
                    {
                        // If the next token is a semicolon, we are terminating a do-while loop
                        // (or we're a while loop with a null statement as its body, which we treat incorrectly)
                        // If the next token is not a semicolon, we are starting a while loop, so we need a newline
                        forceNewlineCount = 1;
                    }
                }
                if (functionDeclarationCountdown > 0)
                    functionDeclarationCountdown--;
            }
            break;
            
            // These tokens are followed by a space, a parenthesized expression, and then a newline
        case EidosTokenType::kTokenIf:
        case EidosTokenType::kTokenFor:
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceSpace = true;
            forceNewlineAfterParenBalance = true;
            break;
            
            // The "do" token starts a do-while loop
        case EidosTokenType::kTokenDo:
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceNewlineCount = 1;
            break;
            
            // The "while" token either ends a do-while loop, or starts a while loop
        case EidosTokenType::kTokenWhile:
            forceSpace = true;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceSpace = true;
            resolveWhileSemanticsAfterParenBalance = true;
            break;
            
            // The "else" token is handled differently depending on whether it is in a ?else expression or an if-else construct
        case EidosTokenType::kTokenElse:
            forceSpace = true;
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceSpace = true;
            if (insideTernaryConditionalCount)
                --insideTernaryConditionalCount;
            else if (NextSignificantToken(tokens, tokenIndex).token_type_ != EidosTokenType::kTokenIf)
                forceNewlineCount = 1;
            break;
            
            // The function token, for user-defined functions, is particularly tricky since it initiates a signature
            // We handle this by going into a special mode that chews through the signature declaration
        case EidosTokenType::kTokenFunction:
            EmitWhitespace(forceSpace, forceNewlineCount, pretty);
            pretty.append(tokenString);
            forceSpace = true;
            functionDeclarationCountdown = 2;   // we will require two close-out right parens to finish the declaration
            break;
        }
        
        if (tokenType != EidosTokenType::kTokenWhitespace)
        {
            lastTokenContainedNewline = false;
            lastTokenSuppressesCommentSpacing = nextLastTokenSuppressesCommentSpacing;
        }
    }
    
    // We're done reformatting; now we need to generate a new script and a new token stream, and fix the indentation of our
    // reformatted string by calling Eidos_prettyprintTokensFromScript(); this avoids duplicating a bunch of logic
    try {
        EidosScript indentScript(pretty);
        
        indentScript.Tokenize(false, true);	// get whitespace and comment tokens
        
        const std::vector<EidosToken> &indentTokens = indentScript.Tokens();
        std::string pretty_indented;
        bool success = false;
        
        success = Eidos_prettyprintTokensFromScript(indentTokens, indentScript, pretty_indented);
        
        if (success)
            pretty = pretty_indented;
        else
            return false;
    }
    catch (...)
    {
        qDebug() << "Reformatted code no longer parsed!";
        return false;
    }
    
    return true;
}






































