//
//  eidos_type_interpreter.cpp
//  Eidos
//
//  Created by Ben Haller on 5/8/16.
//  Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.
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
#include <algorithm>


//
//	EidosTypeInterpreter
//
#pragma mark -
#pragma mark EidosTypeInterpreter
#pragma mark -

EidosTypeInterpreter::EidosTypeInterpreter(const EidosScript &p_script, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only)
: root_node_(p_script.AST()), global_symbols_(&p_symbols), function_map_(p_functions), call_type_map_(p_call_types), defines_only_(p_defines_only)
{
}

EidosTypeInterpreter::EidosTypeInterpreter(const EidosASTNode *p_root_node_, EidosTypeTable &p_symbols, EidosFunctionMap &p_functions, EidosCallTypeTable &p_call_types, bool p_defines_only)
: root_node_(p_root_node_), global_symbols_(&p_symbols), function_map_(p_functions), call_type_map_(p_call_types), defines_only_(p_defines_only)
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

// this is a front end for TypeEvaluateInterpreterBlock() that supports autocompletion of argument names in functions and
// method calls; it just needs to know the position of the end of the script string (where completion is occurring)
EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluateInterpreterBlock_AddArgumentCompletions(std::vector<std::string> *p_argument_completions, size_t p_script_length)
{
	argument_completions_ = p_argument_completions;
	script_length_ = p_script_length;
	
	EidosTypeSpecifier ret = TypeEvaluateInterpreterBlock();
	
	argument_completions_ = nullptr;
	script_length_ = 0;
	
	return ret;
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
			case EidosTokenType::kTokenLParen:		return TypeEvaluate_Call(p_node);
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
			case EidosTokenType::kTokenConditional:	return TypeEvaluate_Conditional(p_node);
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
			case EidosTokenType::kTokenFunction:	return TypeEvaluate_FunctionDecl(p_node);
			default:
				std::cout << "ERROR (EidosTypeInterpreter::TypeEvaluateNode): unexpected node token type " << p_node->token_->token_type_ << "." << EidosTerminate(p_node->token_);
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

EidosTypeSpecifier EidosTypeInterpreter::_TypeEvaluate_FunctionCall_Internal(std::string const &p_function_name, const EidosFunctionSignature *p_function_signature, const std::vector<EidosASTNode *> &p_arguments)
{
#pragma unused(p_function_name)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	int argument_count = (int)p_arguments.size();
	std::vector<EidosTypeSpecifier> argument_types;
	
	// BCH 31 May 2018: We used to not bother doing type-evaluation on arguments, except for the specific cases below where
	// it influences the return type of a function/method, since such evaluation was unlikely to have side effects important
	// for type-evaluation (such as defining new symbols).  Now, however, type-evaluation can define completion symbols for
	// the named arguments of a function/method in argument_completions_ ; see _ProcessArgumentListTypes().  So that this
	// works even doing completion inside nested function/method calls, we have to descend into each argument.  This also
	// makes it so that any other type-evaluation side effects of arguments will occur correctly; it was always a bit of
	// an assumption that no such side effects would exist.  Note that TypeEvaluateNode() is safe to call with nullptr,
	// which is important since p_arguments can contain nullptr for missing/bad arguments.
	for (int argument_index = 0; argument_index < argument_count; ++argument_index)
		argument_types.emplace_back(TypeEvaluateNode(p_arguments[argument_index]));
	
	if (p_function_signature)
	{
		// Look up the result type from the function signature, if there is one
		result_type.type_mask = p_function_signature->return_mask_;
		result_type.object_class = p_function_signature->return_class_;
		
		// We don't call out to functions, but we do have special knowledge of the side effects of built-in Eidos functions.
		// In figuring this stuff out, we need to be careful about the fact that the p_arguments vector can contain nullptr
		// values if there were missing arguments, etc.; we try to be error-tolerant, so we allow cases that would raise
		// in EidosInterpreter.
		EidosInternalFunctionPtr function_ptr = p_function_signature->internal_function_;
		
		if ((function_ptr == &Eidos_ExecuteFunction_defineConstant) && (argument_count == 2))
		{
			// We know that defineConstant() has the side effect of adding a new symbol, and we want to reflect that in
			// our type table so that defined constants are always available.
			if (p_arguments[0])
			{
				if (p_arguments[0]->token_->token_type_ == EidosTokenType::kTokenString)
				{
					const std::string &constant_name = p_arguments[0]->token_->token_string_;
					EidosGlobalStringID constant_id = Eidos_GlobalStringIDForString(constant_name);
					EidosTypeSpecifier &constant_type = argument_types[1];
					
					global_symbols_->SetTypeForSymbol(constant_id, constant_type);
				}
			}
		}
		else if ((argument_count >= 1) && ((function_ptr == &Eidos_ExecuteFunction_rep) ||
										   (function_ptr == &Eidos_ExecuteFunction_repEach) ||
										   (function_ptr == &Eidos_ExecuteFunction_rev) ||
										   (function_ptr == &Eidos_ExecuteFunction_sample) ||
										   (function_ptr == &Eidos_ExecuteFunction_sortBy) ||
										   (function_ptr == &Eidos_ExecuteFunction_unique) ||
										   (function_ptr == &Eidos_ExecuteFunction_setUnion) ||
										   (function_ptr == &Eidos_ExecuteFunction_setIntersection) ||
										   (function_ptr == &Eidos_ExecuteFunction_setDifference) ||
										   (function_ptr == &Eidos_ExecuteFunction_setSymmetricDifference) ||
										   (function_ptr == &Eidos_ExecuteFunction_array) ||
										   (function_ptr == &Eidos_ExecuteFunction_cbind) ||
										   (function_ptr == &Eidos_ExecuteFunction_matrix) ||
										   (function_ptr == &Eidos_ExecuteFunction_matrixMult) ||
										   (function_ptr == &Eidos_ExecuteFunction_rbind) ||
										   (function_ptr == &Eidos_ExecuteFunction_t)))
		{
			// These functions are all defined as returning *, but in fact return the same type/class as their first argument.
			result_type = argument_types[0];
		}
		else if ((function_ptr == &Eidos_ExecuteFunction_ifelse) && (argument_count >= 2))
		{
			// These functions are all defined as returning *, but in fact return the same type/class as their second argument.
			result_type = argument_types[1];
		}
		else if ((function_ptr == &Eidos_ExecuteFunction_c) && (argument_count >= 1))
		{
			// The c() function returns the highest type it is passed (in the sense of promotion order).  This is not
			// important to us, except that if any argument is an object type, we assume the return will mirror that.
			for (int argument_index = 0; argument_index < argument_count; ++argument_index)
			{
				EidosTypeSpecifier &argument_type = argument_types[argument_index];
				
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

EidosTypeSpecifier EidosTypeInterpreter::_TypeEvaluate_MethodCall_Internal(const EidosObjectClass *p_target, const EidosMethodSignature *p_method_signature, const std::vector<EidosASTNode *> &p_arguments)
{
#pragma unused(p_target, p_arguments)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	int argument_count = (int)p_arguments.size();
	std::vector<EidosTypeSpecifier> argument_types;
	
	// BCH 31 May 2018: We used to not bother doing type-evaluation on arguments, except for the specific cases below where
	// it influences the return type of a function/method, since such evaluation was unlikely to have side effects important
	// for type-evaluation (such as defining new symbols).  Now, however, type-evaluation can define completion symbols for
	// the named arguments of a function/method in argument_completions_ ; see _ProcessArgumentListTypes().  So that this
	// works even doing completion inside nested function/method calls, we have to descend into each argument.  This also
	// makes it so that any other type-evaluation side effects of arguments will occur correctly; it was always a bit of
	// an assumption that no such side effects would exist.  Note that TypeEvaluateNode() is safe to call with nullptr,
	// which is important since p_arguments can contain nullptr for missing/bad arguments.
	for (int argument_index = 0; argument_index < argument_count; ++argument_index)
		argument_types.emplace_back(TypeEvaluateNode(p_arguments[argument_index]));
	
	if (p_method_signature)
	{
		// Look up the result type from the method signature, if there is one
		result_type.type_mask = p_method_signature->return_mask_;
		result_type.object_class = p_method_signature->return_class_;
	}
	
	return result_type;
}

void EidosTypeInterpreter::_ProcessArgumentListTypes(const EidosASTNode *p_node, const EidosCallSignature *p_call_signature, std::vector<EidosASTNode *> &p_arguments)
{
	const std::vector<EidosASTNode *> &node_children = p_node->children_;
	
	// BCH 9/12/2020: This is no longer very parallel to _ProcessArgumentList(), because it doesn't involve the argument buffer
	// cache.  It could be brought back up to date, with its own private argument buffer, if necessary, but it seems to work.
	
	// Run through the argument nodes, evaluate them, and put the resulting pointers into the arguments buffer,
	// interleaving default arguments and handling named arguments as we go.
	auto node_children_end = node_children.end();
	int sig_arg_index = 0;
	int sig_arg_count = (int)p_call_signature->arg_name_IDs_.size();
	//bool had_named_argument = false;
	
	for (auto child_iter = node_children.begin() + 1; child_iter != node_children_end; ++child_iter)
	{
		EidosASTNode *child = *child_iter;
		
		if (sig_arg_index < sig_arg_count)
		{
			if (child->token_->token_type_ != EidosTokenType::kTokenAssign)
			{
				// We have a non-named argument; it will go into the next argument slot from the signature
				// In EidosInterpreter it is an error if had_named_argument is set here; here, we ignore that
				
				// If this argument is the very last thing in the script string, then the user is trying to complete on it;
				// in that case, we add potential matches to the completion list, providing autocompletion of argument names.
				// We may be completing off an identifier (with partial typing), or off a bad token (with no typing).
				// That token should be at or after the script end (if it is a bad token it may be immediately after, since
				// it may have gotten its position from an EOF at the end of the token stream).
				// BCH 6 April 2019: We may also be completing off of what appears to be a language keyword, such as "for"
				// trying to complete to "format=".  We want to treat language keywords like identifiers here.
				if (argument_completions_ && (script_length_ <= (size_t)child->token_->token_end_ + 1))
				{
					if ((child->token_->token_type_ == EidosTokenType::kTokenIdentifier) || (child->token_->token_type_ == EidosTokenType::kTokenBad) || (child->token_->token_type_ >= EidosTokenType::kFirstIdentifierLikeToken))
					{
						// Check each argument in the signature as a possibility for completion
						for (int sig_arg_match_index = sig_arg_index; sig_arg_match_index < sig_arg_count; ++sig_arg_match_index)
						{
							const std::string &arg_name = p_call_signature->arg_names_[sig_arg_match_index];
							bool is_ellipsis = (arg_name == "...");
							
							// To be a completion match, the name must not be private API ('_' prefix) or an ellipsis ('...')
							// Whether it is an acceptable completion in other respects will be checked by the completion engine
							if ((arg_name[0] != '_') && !is_ellipsis)
								argument_completions_->push_back(arg_name);
							
							// If the argument we just examined is non-optional, we don't want to offer any further suggestions
							// since they would not be legal to supply in this position in the function/method call.
							// The exception is an ellipsis, which should be treated as optional since it can be skipped over.
							if (!(p_call_signature->arg_masks_[sig_arg_match_index] & kEidosValueMaskOptional) && !is_ellipsis)
								break;
						}
					}
				}
				
				// Advance to the next argument slot unless we're in the ellipsis; only a named argument pops us out of the ellipsis
				if (p_call_signature->arg_name_IDs_[sig_arg_index] != gEidosID_ELLIPSIS)
					sig_arg_index++;
			}
			else
			{
				// We have a named argument; get information on it from its children
				const std::vector<EidosASTNode *> &child_children = child->children_;
				
				if (child_children.size() == 2)	// other than 2 should never happen; raises in EidosInterpreter
				{
					EidosASTNode *named_arg_name_node = child_children[0];
					EidosASTNode *named_arg_value_node = child_children[1];
					
					// Get the identifier for the argument name
					EidosGlobalStringID named_arg_nameID = named_arg_name_node->cached_stringID_;
					
					// Now re-point child at the value node
					child = named_arg_value_node;
					
					// While this argument's name doesn't match the expected argument, insert default values for optional arguments
					do 
					{
						EidosGlobalStringID arg_name_ID = p_call_signature->arg_name_IDs_[sig_arg_index];
						
						if (named_arg_nameID == arg_name_ID)
						{
							sig_arg_index++;
							break;
						}
						
						// In EidosInterpreter it is an error if a named argument skips over a required argument; here we ignore that
						// In EidosInterpreter it is an error if an optional argument has no default; here we ignore that
						
						// arguments that receive the default value are represented in the argument list here with nullptr, since we have no node for them
						// if the signature argument is an ellipsis, however, skip over it with no default value
						if (arg_name_ID != gEidosID_ELLIPSIS)
							p_arguments.emplace_back(nullptr);
						
						// Move to the next signature argument
						sig_arg_index++;
						if (sig_arg_index == sig_arg_count)
							break;		// this is an error in EidosInterpreter; here we just break out to add the named argument after all the signature args
					}
					while (true);
					
					//had_named_argument = true;
				}
			}
		}
		else
		{
			// We're beyond the end of the signature's arguments; in EidosInterpreter this is complicated because of ellipsis args, here we just let it go
		}
		
		// The child pointer is an argument node, so remember it
		p_arguments.emplace_back(child);
	}
	
	// Handle any remaining arguments in the signature
	while (sig_arg_index < sig_arg_count)
	{
		// In EidosInterpreter it is an error if a non-optional argument remains unmatched; here we ignore that
		// In EidosInterpreter it is an error if an optional argument has no default; here we ignore that
		
		// arguments that receive the default value are represented in the argument list here with nullptr, since we have no node for them
		// if the signature argument is an ellipsis, however, skip over it with no default value
		EidosGlobalStringID arg_name_ID = p_call_signature->arg_name_IDs_[sig_arg_index];
		
		if (arg_name_ID != gEidosID_ELLIPSIS)
			p_arguments.emplace_back(nullptr);
		
		sig_arg_index++;
	}
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Call(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	// We do not evaluate the call name node (our first child) to get a function/method object; there is no such type
	// in Eidos for now.  Instead, we extract the identifier name directly from the node and work with it.  If the
	// node is an identifier, it is a function call; if it is a dot operator, it is a method call; other constructs
	// are illegal since expressions cannot evaluate to function/method objects.
	const std::vector<EidosASTNode *> &node_children = p_node->children_;
	EidosASTNode *call_name_node = node_children[0];
	EidosTokenType call_name_token_type = call_name_node->token_->token_type_;
	EidosToken *call_identifier_token = nullptr;
	
	if (call_name_token_type == EidosTokenType::kTokenIdentifier)
	{
		//
		//	FUNCTION CALL DISPATCH
		//
		call_identifier_token = call_name_node->token_;
		
		// OK, we have <identifier>(...); that's a well-formed function call
		const std::string *function_name = &(call_identifier_token->token_string_);
		const EidosFunctionSignature *function_signature = call_name_node->cached_signature_.get();
		
		// If the function call is a built-in Eidos function, we might already have a pointer to its signature cached; if not, we'll have to look it up
		if (!function_signature)
		{
			// Get the function signature
			auto signature_iter = function_map_.find(*function_name);
			
			if (signature_iter != function_map_.end())
				function_signature = signature_iter->second.get();
		}
		
		if (function_signature)
		{
			// Argument processing
			std::vector<EidosASTNode *> arguments;
			
			_ProcessArgumentListTypes(p_node, function_signature, arguments);
			
			// Dispatch to determine the return type
			result_type = _TypeEvaluate_FunctionCall_Internal(*function_name, function_signature, arguments);
			
			// Remember the class returned by function calls, for later use by code completion in cases of ambiguity.
			// See -[EidosTextView completionsForKeyPathEndingInTokenIndex:...] for more background on this.
			if (result_type.object_class)
				call_type_map_.emplace(EidosCallTypeEntry(call_name_node->token_->token_start_, result_type.object_class));
		}
	}
	else if (call_name_token_type == EidosTokenType::kTokenDot)
	{
		//
		//	METHOD CALL DISPATCH
		//
		if (call_name_node->children_.size() >= 2)
		{
			EidosTypeSpecifier first_child_type = TypeEvaluateNode(call_name_node->children_[0]);
			const EidosObjectClass *method_class = first_child_type.object_class;
			
			if (method_class)
			{
				EidosASTNode *second_child_node = call_name_node->children_[1];
				
				if (second_child_node->token_->token_type_ == EidosTokenType::kTokenIdentifier)
				{
					// OK, we have <object type>.<identifier>(...); that's a well-formed method call
					call_identifier_token = second_child_node->token_;
					(void)call_identifier_token;		// tell the static analyzer that we know we just did a dead store
					
					EidosGlobalStringID method_id = second_child_node->cached_stringID_;
					const EidosMethodSignature *method_signature = method_class->SignatureForMethod(method_id);
					
					if (method_signature)
					{
						// Argument processing
						std::vector<EidosASTNode *> arguments;
						
						_ProcessArgumentListTypes(p_node, method_signature, arguments);
						
						// Dispatch; note that we don't treat class and instance methods differently here the way we do in EidosInterpreter
						result_type = _TypeEvaluate_MethodCall_Internal(method_class, method_signature, arguments);
					}
				}
			}
		}
	}
	
	return result_type;
}

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Subset(const EidosASTNode *p_node)
{
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 1)
	{
		result_type = TypeEvaluateNode(p_node->children_[0]);
		
		// No need to evaluate the subset index argument(s), since they cannot define new variables
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

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_Conditional(const EidosASTNode *p_node)
{
	auto children_size = p_node->children_.size();
	
	// In general, the type of a ternary conditional can't be predicted, because the true and
	// false clauses do not have to result in the same type in Eidos, so this is the default
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (children_size == 3)
	{
		EidosASTNode *true_node = p_node->children_[1];
		EidosASTNode *false_node = p_node->children_[2];
		EidosTypeSpecifier true_type = TypeEvaluateNode(true_node);
		EidosTypeSpecifier false_type = TypeEvaluateNode(false_node);
		
		// Try to merge the types of the true and false clauses if possible
		if ((true_type.type_mask == false_type.type_mask) && (true_type.object_class == false_type.object_class))
		{
			// Their types are identical, so that's easy
			return true_type;
		}
		else if (((true_type.type_mask & kEidosValueMaskObject) == kEidosValueMaskObject) && ((false_type.type_mask & kEidosValueMaskObject) == kEidosValueMaskObject))
		{
			// Both types include object, so we can merge them if their object classes match
			result_type.type_mask = (true_type.type_mask | false_type.type_mask);
			
			if (true_type.object_class == false_type.object_class)
				result_type.object_class = true_type.object_class;
			else
				result_type.object_class = nullptr;		// object is included in our type, but we don't know the class
		}
		else if (((true_type.type_mask & kEidosValueMaskObject) == kEidosValueMaskNone) && ((false_type.type_mask & kEidosValueMaskObject) == kEidosValueMaskNone))
		{
			// Neither type includes object, so we can just merge them
			result_type.type_mask = (true_type.type_mask | false_type.type_mask);
		}
		else
		{
			// We have a mix of object and non-object, so we exclude the object type as part of our result;
			// we don't want to guarantee that object is included if it is only present sometimes
			result_type.type_mask = (true_type.type_mask | false_type.type_mask);
			result_type.type_mask &= (~kEidosValueMaskObject);
		}
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
			global_symbols_->SetTypeForSymbol(identifier_name, rvalue_type);
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
	EidosValue_SP result_SP = p_node->cached_literal_value_;
	
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
	// a cached value might be present, from EidosASTNode::_OptimizeConstants(), but at present we don't look
	EidosTypeSpecifier result_type = global_symbols_->GetTypeForSymbol(p_node->cached_stringID_);
	
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
				
				global_symbols_->SetTypeForSymbol(identifier_name, range_type);
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

EidosTypeSpecifier EidosTypeInterpreter::TypeEvaluate_FunctionDecl(const EidosASTNode *p_node)
{
#pragma unused(p_node)
	EidosTypeSpecifier result_type = EidosTypeSpecifier{kEidosValueMaskNone, nullptr};
	
	if (p_node->children_.size() >= 4)
	{
		// We have two jobs to do.  One is to build a function signature for the declared function, so that calls to that function
		// then return the correct type when type-interpreted, just like built-in functions.  We will do that first, so that
		// recursive functions are type-interpreted correctly.
		const EidosASTNode *return_type_node = p_node->children_[0];
		const EidosASTNode *function_name_node = p_node->children_[1];
		const EidosASTNode *param_list_node = p_node->children_[2];
		const EidosASTNode *body_node = p_node->children_[3];
		
		// If we don't have enough of the function declaration to build a signature, don't do anything
		if ((return_type_node->token_->token_type_ == EidosTokenType::kTokenEOF) ||
			(function_name_node->token_->token_type_ == EidosTokenType::kTokenEOF) ||
			(param_list_node->token_->token_type_ == EidosTokenType::kTokenEOF))
			return result_type;
		
		// Build a signature object for this function
		EidosTypeSpecifier &return_type = return_type_node->typespec_;
		const std::string &function_name = function_name_node->token_->token_string_;
		const std::vector<EidosASTNode *> &param_nodes = param_list_node->children_;
		std::vector<std::string> used_param_names;
		
		{
			EidosFunctionSignature *sig;
			
			if (return_type.object_class == nullptr)
				sig = (EidosFunctionSignature *)(new EidosFunctionSignature(function_name,
																			nullptr,
																			return_type.type_mask));
			else
				sig = (EidosFunctionSignature *)(new EidosFunctionSignature(function_name,
																			nullptr,
																			return_type.type_mask,
																			return_type.object_class));
			
			for (EidosASTNode *param_node : param_nodes)
			{
				const std::vector<EidosASTNode *> &param_children = param_node->children_;
				int param_children_count = (int)param_children.size();
				
				if ((param_children_count == 2) || (param_children_count == 3))
				{
					EidosTypeSpecifier &param_type = param_children[0]->typespec_;
					const std::string &param_name = param_children[1]->token_->token_string_;
					
					// Check param_name; it needs to not be used by another parameter
					if (std::find(used_param_names.begin(), used_param_names.end(), param_name) != used_param_names.end())
						continue;
					
					if (param_children_count >= 2)
					{
						// param_node has 2 or 3 children (type, identifier, [default]); we don't care about default values
						
						// Note that we really can't easily deal with default values, because we would have to actually parse the
						// defualt-value node to get a value, if it is an identifier, and then once we have the value we can't
						// easily represent it symbolically any more anyway.  This means function signature previews in the
						// status bar won't show default arguments for user-defined functions; the information to do that will
						// not be gathered here.  Maybe this can be improved at some point.  FIXME BCH 23 October 2017
						
						// we call AddArgWithDefault() so we can specify fault-tolerance; we're not allowed to raise!
						// note this means that erroneous function prototypes will lead to faulty signatures in our function map,
						// but that is fine, it may just mean that an incorrect signature gets previewed to the user
						sig->AddArgWithDefault(param_type.type_mask, param_name, param_type.object_class, EidosValue_SP(nullptr), true);	// true is fault-tolerant
					}
					
					used_param_names.push_back(param_name);
				}
			}
			
			sig->user_defined_ = true;
			
			//std::cout << *sig << std::endl;
			
			// Check that a built-in function is not already defined with this name; no replacing the built-ins.
			auto signature_iter = function_map_.find(function_name);
			bool can_redefine = true;
			
			if (signature_iter != function_map_.end())
			{
				const EidosFunctionSignature *prior_sig = signature_iter->second.get();
				
				if (prior_sig->internal_function_ || !prior_sig->delegate_name_.empty() || !prior_sig->user_defined_)
					can_redefine = false;
			}
			
			if (can_redefine)
			{
				// Add the user-defined function to our function map (possibly replacing a previous version)
				auto found_iter = function_map_.find(sig->call_name_);
				
				if (found_iter != function_map_.end())
					function_map_.erase(found_iter);
				
				function_map_.insert(EidosFunctionMapPair(sig->call_name_, EidosFunctionSignature_CSP(sig)));
			}
			else
			{
				delete sig;
			}
			
			// the signature is now under shared_ptr, or deleted, and so variable sig falls out of scope here
		}
		
		// Our other job is to type-interpret inside the body node of the declared function, with the correct scoping set up so
		// that the parameters of the function are defined inside the body, outer variables are not in scope, etc.  This is
		// different from EidosInterpreter; it is as if the declared function is getting called as it is declared, in a way.
		// This custom type scope for the function lasts only to the end of the declared function, and then we want to forget
		// the custom scope and go back to the type table we were using before, as long as the declaration is complete.
		if (body_node->token_->token_type_ != EidosTokenType::kTokenEOF)
		{
			if (body_node->hit_eof_in_tolerant_parse_)
			{
				// The EOF was hit while parsing the function body, so we're supposed to leave the type table reflecting the scope
				// inside the function.  To do this, we don't create a sub-type-interpreter, we just repurpose our type table.
				global_symbols_->RemoveAllSymbols();
				
				gEidosConstantsSymbolTable->AddSymbolsToTypeTable(global_symbols_);
				
				for (EidosASTNode *param_node : param_nodes)
				{
					const std::vector<EidosASTNode *> &param_children = param_node->children_;
					int param_children_count = (int)param_children.size();
					
					if ((param_children_count == 2) || (param_children_count == 3))
					{
						EidosTypeSpecifier &param_type = param_children[0]->typespec_;
						const std::string &param_name = param_children[1]->token_->token_string_;
						
						global_symbols_->SetTypeForSymbol(Eidos_GlobalStringIDForString(param_name), param_type);
					}
				}
				
				TypeEvaluateNode(body_node);	// result not used
			}
			else
			{
				// The EOF was not hit while parsing the function body, so we're supposed to leave the type table alone in the end.
				// We do this by creating a separate type table that we just use temporarily, inside the function body.  We could
				// almost skip type-interpreting the function body entirely, except that it might define a constant.
				EidosTypeTable typeTable;
				EidosCallTypeTable callTypeTable;
				
				gEidosConstantsSymbolTable->AddSymbolsToTypeTable(&typeTable);
				
				for (EidosASTNode *param_node : param_nodes)
				{
					const std::vector<EidosASTNode *> &param_children = param_node->children_;
					int param_children_count = (int)param_children.size();
					
					if ((param_children_count == 2) || (param_children_count == 3))
					{
						EidosTypeSpecifier &param_type = param_children[0]->typespec_;
						const std::string &param_name = param_children[1]->token_->token_string_;
						
						typeTable.SetTypeForSymbol(Eidos_GlobalStringIDForString(param_name), param_type);
					}
				}
				
				EidosTypeInterpreter typeInterpreter(body_node, typeTable, function_map_, callTypeTable);
				
				typeInterpreter.TypeEvaluateNode(body_node);	// result not used
			}
		}
	}
	
	return result_type;
}













































































