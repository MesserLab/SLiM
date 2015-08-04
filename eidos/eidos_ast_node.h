//
//  eidos_ast_node.h
//  Eidos
//
//  Created by Ben Haller on 7/27/15.
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


#ifndef __Eidos__eidos_ast_node__
#define __Eidos__eidos_ast_node__

#include <stdio.h>
#include <vector>

#include "eidos_token.h"
#include "eidos_value.h"


// A class representing a node in a parse tree for a script
class EidosASTNode
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
public:
	
	EidosToken *token_;											// normally not owned (owned by the Script's token stream) but:
	bool token_is_owned_ = false;								// if T, we own token_ because it is a virtual token that replaced a real token
	std::vector<EidosASTNode *> children_;						// OWNED POINTERS
	
	mutable EidosValue *cached_value_ = nullptr;				// an optional pre-cached EidosValue representing the node
	mutable bool cached_value_is_owned_ = false;				// if T, this node owns its own cached_value_; if F, a descendant node owns it
	mutable const EidosFunctionSignature *cached_signature_ = nullptr;	// NOT OWNED: a cached pointer to the function signature corresponding to the token
	mutable EidosGlobalStringID cached_stringID = gEidosID_none;		// a pre-cached identifier for the token string, for fast property/method lookup
	
	EidosASTNode(const EidosASTNode&) = delete;					// no copying
	EidosASTNode& operator=(const EidosASTNode&) = delete;		// no copying
	EidosASTNode(void) = delete;								// no null construction
	EidosASTNode(EidosToken *p_token, bool p_token_is_owned = false);	// standard constructor; if p_token_is_owned, we own the token
	EidosASTNode(EidosToken *p_token, EidosASTNode *p_child_node);
	
	~EidosASTNode(void);										// destructor
	
	void AddChild(EidosASTNode *p_child_node);					// takes ownership of the passed node
	void ReplaceTokenWithToken(EidosToken *p_token);			// used to fix virtual token to encompass their children; takes ownership
	
	void OptimizeTree(void) const;								// perform various (required) optimizations on the AST
	void _OptimizeConstants(void) const;						// cache EidosValues for constants and propagate constants upward
	void _OptimizeIdentifiers(void) const;						// cache function signatures, global strings for methods and properties, etc.
	
	void PrintToken(std::ostream &p_outstream) const;
	void PrintTreeWithIndent(std::ostream &p_outstream, int p_indent) const;
};


#endif /* defined(__Eidos__eidos_ast_node__) */































































