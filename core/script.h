//
//  script.h
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

/*
 
 The class Script represents a script read from the configuration file that is to be executed for some range of generations.
 For simplicity, this header contains all of the classes involved in scripting SLiM, including Script, ScriptToken, and ScriptASTNode.
 
 */

#ifndef __SLiM__script__
#define __SLiM__script__

#include <vector>
#include <string>


// set these to true to get logging of tokens / AST / evaluation
extern bool gSLiMScriptLogTokens;
extern bool gSLiMScriptLogAST;
extern bool gSLiMScriptLogEvaluation;


// An enumeration for all token types, whether real or virtual
enum class TokenType {
	kTokenNone = 0,
	kTokenEOF,
	kTokenWhitespace,
	
	kTokenSemicolon,	// ;		statement terminator
	kTokenColon,		// :		range operator, as in R
	kTokenComma,		// ,		presently used for separating function parameters only
	kTokenLBrace,		// {		block delimiter
	kTokenRBrace,		// }		block delimiter
	kTokenLParen,		// (		subexpression delimiter
	kTokenRParen,		// )		subexpression delimiter
	kTokenLBracket,		// [		subset operator
	kTokenRBracket,		// ]		subset operator
	kTokenDot,			// .		member operator
	kTokenPlus,			// +		addition operator
	kTokenMinus,		// -		subtraction operator (unary or binary)
	kTokenMod,			// %		modulo operator
	kTokenMult,			// *		multiplication operator
	kTokenExp,			// ^		exponentiation operator
	
	kTokenAnd,			// &		boolean AND
	kTokenOr,			// |		boolean OR
	
	kTokenDiv,			// /		division operator
	kTokenComment,		// //		comment
	kTokenAssign,		// =		assignment
	kTokenEq,			// ==		equality test
	kTokenLt,			// <		less than test
	kTokenLtEq,			// <=		less than or equals test
	kTokenGt,			// >		greater than test
	kTokenGtEq,			// >=		greater than or equals test
	kTokenNot,			// !		boolean NOT
	kTokenNotEq,		// !=		not equals test
	
	kTokenNumber,		//			there is a single numeric token type for both ints and floats, for now at least
	kTokenString,		//			string literals are bounded by double quotes only, and recognize some escapes
	kTokenIdentifier,	//			all valid identifiers that are not keywords or operators
	
	// ----- VIRTUAL TOKENS; THESE WILL HAVE A STRING OF "" AND A LENGTH OF 0
	
	kTokenInterpreterBlock,			// a block of statements executed as a unit in the interpreter
	kTokenSLiMFile,					// a SLiM input file containing zero or more SLiMscript blocks
	kTokenSLiMScriptBlock,			// a SLiM script block with a generation range, optional callback identifier, etc.
	
	// ----- ALL TOKENS AFTER THIS POINT SHOULD BE KEYWORDS MATCHED BY kTokenIdentifier
	
	kFirstIdentifierLikeToken,
	kTokenIf,			// if		conditional
	kTokenElse,			// else		conditional
	kTokenDo,			// do		loop while condition true
	kTokenWhile,		// while	loop while condition true
	kTokenFor,			// for		loop over set
	kTokenIn,			// in		loop over set
	kTokenNext,			// next		loop jump to end
	kTokenBreak,		// break	loop jump to completion
	kTokenReturn,		// return	return a value from the enclosing block
	
	// SLiM keywords
	kTokenFitness,		// fitness		fitness callback definition
	kTokenMateChoice,	// mateChoice	mateChoice callback definition
	kTokenModifyChild	// modifyChild	modifyChild callback definition
};

std::ostream &operator<<(std::ostream &p_outstream, const TokenType p_token_type);


// A class representing a single token read from a script string
class ScriptToken
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
public:
	
	const TokenType token_type_;			// the type of the token; one of the enumeration above
	const std::string token_string_;		// extracted string object for the token
	const int token_start_;					// character position within script_string_
	const int token_end_;					// character position within script_string_
	
	ScriptToken(const ScriptToken&) = delete;					// no copying
	ScriptToken& operator=(const ScriptToken&) = delete;		// no copying
	ScriptToken(void) = delete;									// no null construction
	ScriptToken(TokenType p_token_type, std::string p_token_string, int p_token_start, int p_token_end);
};

std::ostream &operator<<(std::ostream &p_outstream, const ScriptToken &p_token);


// A class representing a node in a parse tree for a script
class ScriptASTNode
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
public:
	
	ScriptToken *token_;										// not owned (owned by the Script's token stream)
																// FIXME: note that virtual tokens are leaked at present
	std::vector<ScriptASTNode *> children_;						// OWNED POINTERS
	
	ScriptASTNode(const ScriptASTNode&) = delete;				// no copying
	ScriptASTNode& operator=(const ScriptASTNode&) = delete;	// no copying
	ScriptASTNode(void) = delete;								// no null construction
	ScriptASTNode(ScriptToken *p_token);						// standard constructor
	ScriptASTNode(ScriptToken *p_token, ScriptASTNode *p_child_node);
	
	~ScriptASTNode(void);										// destructor
	
	void AddChild(ScriptASTNode *p_child_node);					// takes ownership of the passed node
	void ReplaceTokenWithToken(ScriptToken *p_token);			// used to fix virtual token to encompass their children; takes ownership
	
	void PrintToken(std::ostream &p_outstream) const;
	void PrintTreeWithIndent(std::ostream &p_outstream, int p_indent) const;
};


// A class representing an entire script and all associated tokenization and parsing baggage
class Script
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	
	std::string script_string_;		// the full string for the script, from start-brace to the end of the end-brace line
	int start_character_index_;		// the index of the start brace in the SLiMSim script string
	
	std::vector<ScriptToken *> token_stream_;					// OWNED POINTERS
	ScriptASTNode *parse_root_ = nullptr;						// OWNED POINTER
	
	// parsing ivars, valid only during parsing
	int parse_index_;				// index into token_stream_ of the current token
	ScriptToken *current_token_;	// token_stream_[parse_index_]; owned indirectly
	TokenType current_token_type_;	// token_stream_[parse_index_]->token_type_
	
public:
	
	Script(const Script&) = delete;								// no copying
	Script& operator=(const Script&) = delete;					// no copying
	Script(void) = delete;										// no null construction
	Script(std::string p_script_string, int p_start_index);
	
	~Script(void);												// destructor
	
	void Tokenize(bool p_keep_nonsignificant = false);			// generate token stream from script string
	void AddOptionalSemicolon(void);							// add a semicolon to unterminated input like "6+7" so it works in the console
	
	void ParseSLiMFileToAST(void);								// generate AST from token stream for a SLiM input file ( slim_script_block* EOF )
	void ParseInterpreterBlockToAST(void);						// generate AST from token stream for an interpreter block ( statement* EOF )
	
	void PrintTokens(std::ostream &p_outstream) const;
	void PrintAST(std::ostream &p_outstream) const;
	
	inline const std::vector<ScriptToken *> &Tokens(void) const		{ return token_stream_; }
	inline const ScriptASTNode *AST(void) const						{ return parse_root_; }
	
	// Parsing methods; see grammar for definitions
	void Consume();
	void SetErrorPositionFromCurrentToken(void);
	void Match(TokenType p_token_type, std::string p_context);
	
	// Top-level parse methods for SLiM input files
	ScriptASTNode *Parse_SLiMFile(void);
	ScriptASTNode *Parse_SLiMScriptBlock(void);
	
	// Top-level parse method for the SLiMscript interpreter and other contexts
	ScriptASTNode *Parse_InterpreterBlock(void);
	
	// Lower-level parsing
	ScriptASTNode *Parse_CompoundStatement(void);
	ScriptASTNode *Parse_Statement(void);
	ScriptASTNode *Parse_ExprStatement(void);
	ScriptASTNode *Parse_SelectionStatement(void);
	ScriptASTNode *Parse_DoWhileStatement(void);
	ScriptASTNode *Parse_WhileStatement(void);
	ScriptASTNode *Parse_ForStatement(void);
	ScriptASTNode *Parse_JumpStatement(void);
	ScriptASTNode *Parse_Expr(void);
	ScriptASTNode *Parse_AssignmentExpr(void);
	ScriptASTNode *Parse_LogicalOrExpr(void);
	ScriptASTNode *Parse_LogicalAndExpr(void);
	ScriptASTNode *Parse_EqualityExpr(void);
	ScriptASTNode *Parse_RelationalExpr(void);
	ScriptASTNode *Parse_AddExpr(void);
	ScriptASTNode *Parse_MultExpr(void);
	ScriptASTNode *Parse_SeqExpr(void);
	ScriptASTNode *Parse_ExpExpr(void);
	ScriptASTNode *Parse_UnaryExpr(void);
	ScriptASTNode *Parse_PostfixExpr(void);
	ScriptASTNode *Parse_PrimaryExpr(void);
	ScriptASTNode *Parse_ArgumentExprList(void);
	ScriptASTNode *Parse_Constant(void);
};


#endif /* defined(__SLiM__script__) */



































































