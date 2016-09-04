//
//  eidos_type_interpreter.cpp
//  Eidos
//
//  Created by Ben Haller on 5/8/16.
//  Copyright (c) 2016 Philipp Messer.  All rights reserved.
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


#include "eidos_type_interpreter.h"
#include "eidos_functions.h"
#include "eidos_ast_node.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <sstream>
#include <string>
#include <vector>


using std::string;
using std::vector;
using std::endl;
using std::istringstream;
using std::istream;
using std::ostream;


//
//	EidosTypeInterpreter
//
#pragma mark EidosTypeInterpreter

EidosTypeInterpreter::EidosTypeInterpreter(const EidosScript &p_script, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only)
: root_node_(p_script.AST()), global_symbols_(p_symbols), function_map_(p_functions), call_type_map_(p_call_types), defines_only_(p_defines_only)
{
}

EidosTypeInterpreter::EidosTypeInterpreter(const EidosASTNode *p_root_node_, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only)
: root_node_(p_root_node_), global_symbols_(p_symbols), function_map_(p_functions), call_type_map_(p_call_types), defines_only_(p_defines_only)
{
}

EidosTypeInterpreter::~EidosTypeInterpreter(void)
{
}

// the starting point for script blocks in Eidos, which do not require braces; this is not really a "block" but a series of
// independent statements grouped only by virtue of having been executed together as a unit in the interpreter
EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluateInterpreterBlock()
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	for (EidosASTNode *child_node : root_node_->children_)
		result_type = TypeEvaluateNode(child_node);
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluateNode(const EidosASTNode *p_node)
{
	if (p_node)
	{
		switch (p_node->token_->token_type_)
		{
			case EidosTokenType::kTokenBad:			return EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
			case EidosTokenType::kTokenSemicolon:	return TypeEvaluate_NullStatement(p_node);
			case EidosTokenType::kTokenColon:		return TypeEvaluate_RangeExpr(p_node);
			case EidosTokenType::kTokenLBrace:		return TypeEvaluate_CompoundStatement(p_node);
			case EidosTokenType::kTokenLParen:		return TypeEvaluate_FunctionCall(p_node);
			case EidosTokenType::kTokenLBracket:	return TypeEvaluate_Subset(p_node);
			case EidosTokenType::kTokenDot:			return TypeEvaluate_MemberRef(p_node);
			case EidosTokenType::kTokenPlus:		return TypeEvaluate_Plus(p_node);
			case EidosTokenType::kTokenMinus:		return TypeEvaluate_Minus(p_node);
			case EidosTokenType::kTokenMod:			return TypeEvaluate_Mod(p_node);
			case EidosTokenType::kTokenMult:		return TypeEvaluate_Mult(p_node);
			case EidosTokenType::kTokenExp:			return TypeEvaluate_Exp(p_node);
			case EidosTokenType::kTokenAnd:			return TypeEvaluate_And(p_node);
			case EidosTokenType::kTokenOr:			return TypeEvaluate_Or(p_node);
			case EidosTokenType::kTokenDiv:			return TypeEvaluate_Div(p_node);
			case EidosTokenType::kTokenAssign:		return TypeEvaluate_Assign(p_node);
			case EidosTokenType::kTokenEq:			return TypeEvaluate_Eq(p_node);
			case EidosTokenType::kTokenLt:			return TypeEvaluate_Lt(p_node);
			case EidosTokenType::kTokenLtEq:		return TypeEvaluate_LtEq(p_node);
			case EidosTokenType::kTokenGt:			return TypeEvaluate_Gt(p_node);
			case EidosTokenType::kTokenGtEq:		return TypeEvaluate_GtEq(p_node);
			case EidosTokenType::kTokenNot:			return TypeEvaluate_Not(p_node);
			case EidosTokenType::kTokenNotEq:		return TypeEvaluate_NotEq(p_node);
			case EidosTokenType::kTokenNumber:		return TypeEvaluate_Number(p_node);
			case EidosTokenType::kTokenString:		return TypeEvaluate_String(p_node);
			case EidosTokenType::kTokenIdentifier:	return TypeEvaluate_Identifier(p_node);
			case EidosTokenType::kTokenIf:			return TypeEvaluate_If(p_node);
			case EidosTokenType::kTokenDo:			return TypeEvaluate_Do(p_node);
			case EidosTokenType::kTokenWhile:		return TypeEvaluate_While(p_node);
			case EidosTokenType::kTokenFor:			return TypeEvaluate_For(p_node);
			case EidosTokenType::kTokenNext:		return TypeEvaluate_Next(p_node);
			case EidosTokenType::kTokenBreak:		return TypeEvaluate_Break(p_node);
			case EidosTokenType::kTokenReturn:		return TypeEvaluate_Return(p_node);
			default:
				std::cout << "ERROR (EidosTypeInterpreter::TypeEvaluateNode): unexpected node token type " << p_node->token_->token_type_ << "." << eidos_terminate(p_node->token_);
		}
	}
	
	return EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_NullStatement(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	return EidosTypeSpecifier{kEidosValueMaskNULL, nullptr};
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_CompoundStatement(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNULL, nullptr};
	
	for (EidosASTNode *child_node : p_node->children_)
		result_type = TypeEvaluateNode(child_node);
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_RangeExpr(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		const EidosASTNode *child0 = p_node->children_[0];
		const EidosASTNode *child1 = p_node->children_[1];
		
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(child0);
		EidosTypeSpecifier second_child_type = TypeEvaluateNode(child1);
		
		// If both operands are definitely integer, not float, then the return type is integer.  Otherwise,
		// if both are numeric then the return type is guessed to be float.  Otherwise, none.
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		bool integer2 = !!(second_child_type.type_mask & kEidosValueMaskInt);
		bool float2 = !!(second_child_type.type_mask & kEidosValueMaskFloat);
		
		if ((integer1 && !float1) && (integer2 && !float2))
			result_type.type_mask = kEidosValueMaskInt;
		else if ((!integer1 && float1) || (!integer2 && float2))
			result_type.type_mask = kEidosValueMaskFloat;
		else if ((integer1 || float1) && (integer2 || float2))
			result_type.type_mask = kEidosValueMaskNumeric;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::_TypeEvaluate_FunctionCall_Internal(string const &p_function_name, const EidosFunctionSignature *p_function_signature, const EidosASTNode ** p_arguments, int p_argument_count)
{
#pragma unused(p_function_name)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_function_signature)
	{
		result_type.type_mask = p_function_signature->return_mask_;
		result_type.object_class = p_function_signature->return_class_;
		
		// We don't call out to functions, but we do have special knowledge of the side effects of built-in Eidos functions.
		EidosFunctionIdentifier function_id = p_function_signature->function_id_;
		
		if ((function_id == EidosFunctionIdentifier::defineConstantFunction) && (p_argument_count == 2))
		{
			// We know that defineConstant() has the side effect of adding a new symbol, and we want to reflect that in
			// our type table so that defined constants are always available.
			if (p_arguments[0]->token_->token_type_ == EidosTokenType::kTokenString)
			{
				const std::string &constant_name = p_arguments[0]->token_->token_string_;
				EidosGlobalStringID constant_id = EidosGlobalStringIDForString(constant_name);
				EidosTypeSpecifier constant_type = TypeEvaluateNode(p_arguments[1]);
				
				if (constant_type.object_class == nullptr)
					global_symbols_.SetTypeForSymbol(constant_id, constant_type);
			}
		}
		else if (((function_id == EidosFunctionIdentifier::repFunction) || (function_id == EidosFunctionIdentifier::repEachFunction) || (function_id == EidosFunctionIdentifier::revFunction) || (function_id == EidosFunctionIdentifier::sampleFunction) || (function_id == EidosFunctionIdentifier::sortByFunction) || (function_id == EidosFunctionIdentifier::uniqueFunction)) && (p_argument_count >= 1))
		{
			// These functions are all defined as returning *, but in fact return the same type/class as their first argument.
			EidosTypeSpecifier argument_type = TypeEvaluateNode(p_arguments[0]);
			
			result_type = argument_type;
		}
		else if ((function_id == EidosFunctionIdentifier::ifelseFunction) && (p_argument_count >= 2))
		{
			// These functions are all defined as returning *, but in fact return the same type/class as their second argument.
			EidosTypeSpecifier argument_type = TypeEvaluateNode(p_arguments[1]);
			
			result_type = argument_type;
		}
		else if ((function_id == EidosFunctionIdentifier::cFunction) && (p_argument_count >= 1))
		{
			// The c() function returns the highest type it is passed (in the sense of promotion order).  This is not
			// important to us, except that if any argument is an object type, we assume the return will mirror that.
			for (int argument_index = 0; argument_index < p_argument_count; ++argument_index)
			{
				EidosTypeSpecifier argument_type = TypeEvaluateNode(p_arguments[argument_index]);
				
				if ((argument_type.type_mask & kEidosValueMaskObject) == kEidosValueMaskObject)
				{
					result_type = argument_type;
					break;
				}
			}
		}
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::_TypeEvaluate_MethodCall_Internal(const EidosObjectClass *p_target, const EidosMethodSignature *p_method_signature, const EidosASTNode ** p_arguments, int p_argument_count)
{
#pragma unused(p_target, p_arguments, p_argument_count)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	// We just look up the result type from the method signature, if there is one
	if (p_method_signature)
	{
		result_type.type_mask = p_method_signature->return_mask_;
		result_type.object_class = p_method_signature->return_class_;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_FunctionCall(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	// We do not evaluate the function name node (our first child) to get a function object; there is no such type in
	// Eidos for now.  Instead, we extract the identifier name directly from the node and work with it.  If the
	// node is an identifier, it is a function call; if it is a dot operator, it is a method call; other constructs
	// are illegal since expressions cannot evaluate to function objects, there being no function objects in Eidos.
	EidosASTNode *function_name_node = p_node->children_[0];
	EidosTokenType function_name_token_type = function_name_node->token_->token_type_;
	
	const string *function_name = nullptr;
	const EidosFunctionSignature *function_signature = nullptr;
	EidosGlobalStringID method_id = gEidosID_none;
	const EidosObjectClass *method_class = nullptr;
	const EidosMethodSignature *method_signature = nullptr;
	EidosToken *call_identifier_token = nullptr;
	
	if (function_name_token_type == EidosTokenType::kTokenIdentifier)
	{
		// OK, we have <identifier>(...); that's a well-formed function call
		call_identifier_token = function_name_node->token_;
		function_name = &(call_identifier_token->token_string_);
		function_signature = function_name_node->cached_signature_;
		
		// If the function call is a built-in Eidos function, we might already have a pointer to its signature cached; if not, we'll have to look it up
		// This matches the code at the beginning of ExecuteFunctionCall(); at present functions added to the base map don't get their signature cached
		if (!function_signature)
		{
			// Get the function signature
			auto signature_iter = function_map_.find(*function_name);
			
			if (signature_iter != function_map_.end())
				function_signature = signature_iter->second;
		}
	}
	else if (function_name_token_type == EidosTokenType::kTokenDot)
	{
		if (function_name_node->children_.size() >= 2)
		{
			EidosTypeSpecifier first_child_type = TypeEvaluateNode(function_name_node->children_[0]);
			method_class = first_child_type.object_class;
			
			if (method_class)
			{
				EidosASTNode *second_child_node = function_name_node->children_[1];
				
				if (second_child_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
				{
					// OK, we have <object type>.<identifier>(...); that's a well-formed method call
					call_identifier_token = second_child_node->token_;
					method_id = second_child_node->cached_stringID_;
					method_signature = method_class->SignatureForMethod(method_id);
				}
			}
		}
	}
	
	if (call_identifier_token)
	{
		const std::vector<EidosASTNode *> &node_children = p_node->children_;
		auto node_children_end = node_children.end();
		int arguments_count = 0;
		
		// We use a vector for argument-passing; speed is not a concern here the way it is in EidosInterpreter
		vector<EidosASTNode *> arguments;
		
		for (auto child_iter = node_children.begin() + 1; child_iter != node_children_end; ++child_iter)
		{
			EidosASTNode *child = *child_iter;
			
			if (child->token_->token_type_ == EidosTokenType::kTokenComma)
			{
				// a child with token type kTokenComma is an argument list node; we need to take its children and evaluate them
				std::vector<EidosASTNode *> &child_children = child->children_;
				auto end_iterator = child_children.end();
				
				for (auto arg_list_iter = child_children.begin(); arg_list_iter != end_iterator; ++arg_list_iter)
					arguments.emplace_back(*arg_list_iter);
				
				arguments_count += child->children_.size();
			}
			else
			{
				// all other children get evaluated, and the results added to the arguments vector
				arguments.emplace_back(child);
				
				arguments_count++;
			}
		}
		
		// We offload the actual work to ExecuteMethodCall() / ExecuteFunctionCall() to keep things simple here
		const EidosASTNode **arguments_ptr = (const EidosASTNode **)arguments.data();	// why is this cast needed??
		
		if (method_class)
			result_type = _TypeEvaluate_MethodCall_Internal(method_class, method_signature, arguments_ptr, arguments_count);
		else
			result_type = _TypeEvaluate_FunctionCall_Internal(*function_name, function_signature, arguments_ptr, arguments_count);
		
		// Remember the class returned by function calls, for later use by code completion in cases of ambiguity.
		// See -[EidosTextView completionsForKeyPathEndingInTokenIndex:...] for more background on this.
		if ((!method_class) && result_type.object_class)
			call_type_map_.emplace(EidosCallTypeEntry(function_name_node->token_->token_start_, result_type.object_class));
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Subset(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 1)
	{
		result_type = TypeEvaluateNode(p_node->children_[0]);
		
		// No need to evaluate the subset index argument, it cannot define new variables
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_MemberRef(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		
		if (first_child_type.object_class)
		{
			EidosASTNode *second_child_node = p_node->children_[1];
			EidosToken *second_child_token = second_child_node->token_;
			
			if (second_child_token->token_type_ == EidosTokenType::kTokenIdentifier)
			{
				EidosGlobalStringID property_string_ID = second_child_node->cached_stringID_;
				const EidosPropertySignature *property_signature = first_child_type.object_class->SignatureForProperty(property_string_ID);
				
				if (property_signature)
				{
					result_type.type_mask = property_signature->value_mask_;
					result_type.object_class = property_signature->value_class_;
				}
			}
		}
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Plus(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() == 1)
	{
		// unary plus is a no-op, but legal only for numeric types
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		
		if (integer1 && !float1)
			result_type.type_mask = kEidosValueMaskInt;
		else if (float1 && !integer1)
			result_type.type_mask = kEidosValueMaskFloat;
		else if (integer1 && float1)
			result_type.type_mask = kEidosValueMaskNumeric;
	}
	else if (p_node->children_.size() >= 2)
	{
		// binary plus is legal either between two numeric types, or between a string and any other non-NULL operand
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		EidosTypeSpecifier second_child_type = TypeEvaluateNode(p_node->children_[1]);
		
		if ((first_child_type.type_mask == kEidosValueMaskString) || (second_child_type.type_mask == kEidosValueMaskString))
		{
			result_type.type_mask = kEidosValueMaskString;
		}
		else
		{
			bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
			bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
			bool integer2 = !!(second_child_type.type_mask & kEidosValueMaskInt);
			bool float2 = !!(second_child_type.type_mask & kEidosValueMaskFloat);
			
			if ((integer1 && !float1) && (integer2 && !float2))
				result_type.type_mask = kEidosValueMaskInt;
			else if ((!integer1 && float1) || (!integer2 && float2))
				result_type.type_mask = kEidosValueMaskFloat;
			else if ((integer1 || float1) && (integer2 || float2))
				result_type.type_mask = kEidosValueMaskNumeric;
		}
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Minus(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() == 1)
	{
		// unary minus is a no-op, but legal only for numeric types
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		
		if (integer1 && !float1)
			result_type.type_mask = kEidosValueMaskInt;
		else if (float1 && !integer1)
			result_type.type_mask = kEidosValueMaskFloat;
		else if (integer1 && float1)
			result_type.type_mask = kEidosValueMaskNumeric;
	}
	else if (p_node->children_.size() >= 2)
	{
		// binary minus is legal between two numeric types
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		EidosTypeSpecifier second_child_type = TypeEvaluateNode(p_node->children_[1]);
		
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		bool integer2 = !!(second_child_type.type_mask & kEidosValueMaskInt);
		bool float2 = !!(second_child_type.type_mask & kEidosValueMaskFloat);
		
		if ((integer1 && !float1) && (integer2 && !float2))
			result_type.type_mask = kEidosValueMaskInt;
		else if ((!integer1 && float1) || (!integer2 && float2))
			result_type.type_mask = kEidosValueMaskFloat;
		else if ((integer1 || float1) && (integer2 || float2))
			result_type.type_mask = kEidosValueMaskNumeric;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Mod(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		// modulo is legal between two numeric types, and always produces float
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		EidosTypeSpecifier second_child_type = TypeEvaluateNode(p_node->children_[1]);
		
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		bool integer2 = !!(second_child_type.type_mask & kEidosValueMaskInt);
		bool float2 = !!(second_child_type.type_mask & kEidosValueMaskFloat);
		
		if ((integer1 || float1) && (integer2 || float2))
			result_type.type_mask = kEidosValueMaskFloat;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Mult(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		// multiplication is legal between two numeric types
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		EidosTypeSpecifier second_child_type = TypeEvaluateNode(p_node->children_[1]);
		
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		bool integer2 = !!(second_child_type.type_mask & kEidosValueMaskInt);
		bool float2 = !!(second_child_type.type_mask & kEidosValueMaskFloat);
		
		if ((integer1 && !float1) && (integer2 && !float2))
			result_type.type_mask = kEidosValueMaskInt;
		else if ((!integer1 && float1) || (!integer2 && float2))
			result_type.type_mask = kEidosValueMaskFloat;
		else if ((integer1 || float1) && (integer2 || float2))
			result_type.type_mask = kEidosValueMaskNumeric;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Div(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		// division is legal between two numeric types, and always produces float
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		EidosTypeSpecifier second_child_type = TypeEvaluateNode(p_node->children_[1]);
		
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		bool integer2 = !!(second_child_type.type_mask & kEidosValueMaskInt);
		bool float2 = !!(second_child_type.type_mask & kEidosValueMaskFloat);
		
		if ((integer1 || float1) && (integer2 || float2))
			result_type.type_mask = kEidosValueMaskFloat;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Exp(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		// exponentiation is legal between two numeric types, and always produces float
		EidosTypeSpecifier first_child_type = TypeEvaluateNode(p_node->children_[0]);
		EidosTypeSpecifier second_child_type = TypeEvaluateNode(p_node->children_[1]);
		
		bool integer1 = !!(first_child_type.type_mask & kEidosValueMaskInt);
		bool float1 = !!(first_child_type.type_mask & kEidosValueMaskFloat);
		bool integer2 = !!(second_child_type.type_mask & kEidosValueMaskInt);
		bool float2 = !!(second_child_type.type_mask & kEidosValueMaskFloat);
		
		if ((integer1 || float1) && (integer2 || float2))
			result_type.type_mask = kEidosValueMaskFloat;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_And(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Or(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Not(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Assign(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		EidosASTNode *lvalue_node = p_node->children_[0];
		EidosTypeSpecifier rvalue_type = TypeEvaluateNode(p_node->children_[1]);
		
		// We are only interested in assignments in simple identifier lvalues; other assignments do not alter the type table
		if (lvalue_node->token_->token_type_ != EidosTokenType::kTokenIdentifier)
			return result_type;
		
		EidosGlobalStringID identifier_name = lvalue_node->cached_stringID_;
		
		if (!defines_only_)
			global_symbols_.SetTypeForSymbol(identifier_name, rvalue_type);
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Eq(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Lt(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_LtEq(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Gt(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_GtEq(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_NotEq(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskLogical, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Number(const EidosASTNode *p_node)
{
	// use a cached value from EidosASTNode::_OptimizeConstants() if present; this should always be hit now!
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNumeric, nullptr};
	EidosValue_SP result_SP = p_node->cached_value_;
	
	if (result_SP)
	{
		EidosValueType type = result_SP->Type();
		
		if (type == EidosValueType::kValueInt)
			result_type.type_mask = kEidosValueMaskInt;
		else if (type == EidosValueType::kValueFloat)
			result_type.type_mask = kEidosValueMaskFloat;
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_String(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskString, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Identifier(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = global_symbols_.GetTypeForSymbol(p_node->cached_stringID_);
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_If(const EidosASTNode *p_node)
{
	auto children_size = p_node->children_.size();
	
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (children_size > 1)
	{
		EidosASTNode *true_node = p_node->children_[1];
		TypeEvaluateNode(true_node);
	}
	
	if (children_size > 2)
	{
		EidosASTNode *false_node = p_node->children_[2];
		TypeEvaluateNode(false_node);
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Do(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 1)
		TypeEvaluateNode(p_node->children_[0]);
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_While(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
		TypeEvaluateNode(p_node->children_[1]);
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_For(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 2)
	{
		EidosASTNode *identifier_child = p_node->children_[0];
		const EidosASTNode *range_node = p_node->children_[1];
		EidosTypeSpecifier range_type = TypeEvaluateNode(range_node);
		
		// we require an identifier to assign into; I toyed with allowing any lvalue, but that is kind of weird / complicated...
		if (identifier_child->token_->token_type_ == EidosTokenType::kTokenIdentifier)
		{
			if (!defines_only_)
			{
				EidosGlobalStringID identifier_name = identifier_child->cached_stringID_;
				
				global_symbols_.SetTypeForSymbol(identifier_name, range_type);
			}
		}
	}
	
	if (p_node->children_.size() >= 3)
		TypeEvaluateNode(p_node->children_[2]);
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Next(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Break(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Return(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNULL, nullptr};
	
	if (p_node->children_.size() >= 1)
		result_type = TypeEvaluateNode(p_node->children_[0]);
	
	return result_type;
}














































































