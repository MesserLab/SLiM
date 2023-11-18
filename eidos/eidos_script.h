//
//  eidos_script.h
//  Eidos
//
//  Created by Ben Haller on 4/1/15.
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

/*
 
 The class EidosScript represents a script written in the Eidos language.  It handles its tokenizing and parsing itself.
 
 */

#ifndef __Eidos__eidos_script__
#define __Eidos__eidos_script__

#include <vector>
#include <string>

#include "eidos_token.h"


class EidosValue;
class EidosASTNode;


// set these to true to get logging of tokens / AST / evaluation
extern bool gEidosLogTokens;
extern bool gEidosLogAST;
extern bool gEidosLogEvaluation;


// A class representing an entire script and all associated tokenization and parsing baggage
class EidosScript
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
protected:
	
	const std::string script_string_;		// the full string for the script, from start-brace to the end of the end-brace line
	int32_t user_script_line_offset_;		// the initial position (lines) in the user's script; -1 if it is not in the user's script
	
	std::vector<EidosToken> token_stream_;
	EidosASTNode *parse_root_ = nullptr;						// OWNED POINTER
	
	bool final_semicolon_optional_ = false;
	
	// parsing ivars, valid only during parsing
	int parse_index_;						// index into token_stream_ of the current token
	EidosToken *current_token_;				// token_stream_[parse_index_]; owned indirectly
	EidosTokenType current_token_type_;		// token_stream_[parse_index_]->token_type_
	bool parse_make_bad_nodes_ = false;		// if true, do error-tolerant parsing and introduce dummy nodes as needed
	
public:
	
	EidosScript(const EidosScript&) = delete;								// no copying
	EidosScript& operator=(const EidosScript&) = delete;					// no copying
	EidosScript(void) = delete;												// no null construction
	
	// Constructing a script now requires the script string to be located in the context of the user's full script string,
	// to allow debug points set on specific line numbers in the full script to work; there is now a line offset, used to
	// translate points in this script into points in the full script.  Pass -1 for scripts that are not based in the
	// user's full script string (lambdas, etc.).
	explicit EidosScript(std::string p_script_string, int32_t p_user_script_line_offset);
	
	virtual ~EidosScript(void);
	
	void SetFinalSemicolonOptional(bool p_optional_semicolon)		{ final_semicolon_optional_ = p_optional_semicolon; }
	inline int32_t UserScriptLineOffset(void) const { return user_script_line_offset_; }
	
	// generate token stream from script string; if p_make_bad_tokens == true this function will not raise or fail
	void Tokenize(bool p_make_bad_tokens = false, bool p_keep_nonsignificant = false);
	
	// generate AST from token stream for an interpreter block ( statement* EOF )
	void ParseInterpreterBlockToAST(bool p_allow_functions, bool p_make_bad_nodes = false);
	
	void PrintTokens(std::ostream &p_outstream) const;
	void PrintAST(std::ostream &p_outstream) const;
	
	inline __attribute__((always_inline)) const std::string &String(void) const					{ return script_string_; }
	inline __attribute__((always_inline)) const std::vector<EidosToken> &Tokens(void) const		{ return token_stream_; }
	inline __attribute__((always_inline)) const EidosASTNode *AST(void) const					{ return parse_root_; }
	
	// Parsing methods; see grammar for definitions
	void Consume(void);
	void Match(EidosTokenType p_token_type, const char *p_context_cstr);
	
	// Top-level parse method for the Eidos interpreter and other contexts
	EidosASTNode *Parse_InterpreterBlock(bool p_allow_functions);
	
	// Lower-level parsing
	EidosASTNode *Parse_CompoundStatement(void);
	EidosASTNode *Parse_Statement(void);
	EidosASTNode *Parse_ExprStatement(void);
	EidosASTNode *Parse_SelectionStatement(void);
	EidosASTNode *Parse_DoWhileStatement(void);
	EidosASTNode *Parse_WhileStatement(void);
	EidosASTNode *Parse_ForStatement(void);
	EidosASTNode *Parse_JumpStatement(void);
	EidosASTNode *Parse_Expr(void);
	EidosASTNode *Parse_AssignmentExpr(void);
	EidosASTNode *Parse_ConditionalExpr(void);
	EidosASTNode *Parse_LogicalOrExpr(void);
	EidosASTNode *Parse_LogicalAndExpr(void);
	EidosASTNode *Parse_EqualityExpr(void);
	EidosASTNode *Parse_RelationalExpr(void);
	EidosASTNode *Parse_AddExpr(void);
	EidosASTNode *Parse_MultExpr(void);
	EidosASTNode *Parse_SeqExpr(void);
	EidosASTNode *Parse_UnaryExpExpr(void);
	EidosASTNode *Parse_PostfixExpr(void);
	EidosASTNode *Parse_PrimaryExpr(void);
	void Parse_ArgumentExprList(EidosASTNode *p_parent_node);	// adds to the parent node
	EidosASTNode *Parse_ArgumentExpr(void);
	EidosASTNode *Parse_Constant(void);
	
	EidosASTNode *Parse_FunctionDecl(void);
	EidosASTNode *Parse_ReturnTypeSpec();
	EidosASTNode *Parse_TypeSpec(void);
	EidosASTNode *Parse_ObjectClassSpec(EidosASTNode *p_type_node);	// adds to type node
	EidosASTNode *Parse_ParamList(void);
	EidosASTNode *Parse_ParamSpec(void);
	EidosASTNode *Parse_DefaultValue(void);
};


#endif /* defined(__Eidos__eidos_script__) */



































































