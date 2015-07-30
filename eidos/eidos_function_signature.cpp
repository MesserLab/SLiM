//
//  eidos_function_signature.cpp
//  Eidos
//
//  Created by Ben Haller on 5/16/15.
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


#include "eidos_function_signature.h"


using std::string;
using std::vector;
using std::endl;
using std::ostream;


//
//	EidosFunctionSignature
//
#pragma mark EidosFunctionSignature

EidosFunctionSignature::EidosFunctionSignature(const string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask) :
function_name_(p_function_name), function_id_(p_function_id), return_mask_(p_return_mask)
{
}

EidosFunctionSignature::EidosFunctionSignature(const string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, EidosDelegateFunctionPtr p_delegate_function, void *p_delegate_object, const string &p_delegate_name) :
function_name_(p_function_name), function_id_(p_function_id), return_mask_(p_return_mask), delegate_function_(p_delegate_function), delegate_object_(p_delegate_object), delegate_name_(p_delegate_name)
{
}

EidosFunctionSignature *EidosFunctionSignature::SetClassMethod()
{
	is_class_method = true;
	return this;
}

EidosFunctionSignature *EidosFunctionSignature::SetInstanceMethod()
{
	is_instance_method = true;
	return this;
}

EidosFunctionSignature *EidosFunctionSignature::AddArg(EidosValueMask p_arg_mask, const std::string &p_argument_name)
{
	bool is_optional = !!(p_arg_mask & kValueMaskOptional);
	
	if (has_optional_args_ && !is_optional)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::AddArg): cannot add a required argument after an optional argument has been added." << eidos_terminate();
	
	if (has_ellipsis_)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::AddArg): cannot add an argument after an ellipsis." << eidos_terminate();
	
	if (p_argument_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::AddArg): an argument name is required." << eidos_terminate();
	
	arg_masks_.push_back(p_arg_mask);
	arg_names_.push_back(p_argument_name);
	
	if (is_optional)
		has_optional_args_ = true;
	
	return this;
}

EidosFunctionSignature *EidosFunctionSignature::AddEllipsis()
{
	if (has_optional_args_)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::AddEllipsis): cannot add an ellipsis after an optional argument has been added." << eidos_terminate();
	
	if (has_ellipsis_)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::AddEllipsis): cannot add more than one ellipsis." << eidos_terminate();
	
	has_ellipsis_ = true;
	
	return this;
}

EidosFunctionSignature *EidosFunctionSignature::AddLogical(const std::string &p_argument_name)			{ return AddArg(kValueMaskLogical, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt(const std::string &p_argument_name)				{ return AddArg(kValueMaskInt, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat(const std::string &p_argument_name)			{ return AddArg(kValueMaskFloat, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString(const std::string &p_argument_name)			{ return AddArg(kValueMaskString, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject(const std::string &p_argument_name)			{ return AddArg(kValueMaskObject, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric(const std::string &p_argument_name)			{ return AddArg(kValueMaskNumeric, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogicalEquiv, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase(const std::string &p_argument_name)			{ return AddArg(kValueMaskAnyBase, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAny(const std::string &p_argument_name)				{ return AddArg(kValueMaskAny, p_argument_name); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_O(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogical | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_O(const std::string &p_argument_name)			{ return AddArg(kValueMaskInt | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_O(const std::string &p_argument_name)			{ return AddArg(kValueMaskFloat | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString_O(const std::string &p_argument_name)			{ return AddArg(kValueMaskString | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_O(const std::string &p_argument_name)			{ return AddArg(kValueMaskObject | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_O(const std::string &p_argument_name)		{ return AddArg(kValueMaskNumeric | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_O(const std::string &p_argument_name)	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase_O(const std::string &p_argument_name)		{ return AddArg(kValueMaskAnyBase | kValueMaskOptional, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAny_O(const std::string &p_argument_name)			{ return AddArg(kValueMaskAny | kValueMaskOptional, p_argument_name); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_S(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogical | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_S(const std::string &p_argument_name)			{ return AddArg(kValueMaskInt | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_S(const std::string &p_argument_name)			{ return AddArg(kValueMaskFloat | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString_S(const std::string &p_argument_name)			{ return AddArg(kValueMaskString | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_S(const std::string &p_argument_name)			{ return AddArg(kValueMaskObject | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_S(const std::string &p_argument_name)		{ return AddArg(kValueMaskNumeric | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_S(const std::string &p_argument_name)	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase_S(const std::string &p_argument_name)		{ return AddArg(kValueMaskAnyBase | kValueMaskSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAny_S(const std::string &p_argument_name)			{ return AddArg(kValueMaskAny | kValueMaskSingleton, p_argument_name); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_OS(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogical | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_OS(const std::string &p_argument_name)			{ return AddArg(kValueMaskInt | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_OS(const std::string &p_argument_name)			{ return AddArg(kValueMaskFloat | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString_OS(const std::string &p_argument_name)		{ return AddArg(kValueMaskString | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_OS(const std::string &p_argument_name)		{ return AddArg(kValueMaskObject | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_OS(const std::string &p_argument_name)		{ return AddArg(kValueMaskNumeric | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_OS(const std::string &p_argument_name)	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase_OS(const std::string &p_argument_name)		{ return AddArg(kValueMaskAnyBase | kValueMaskOptSingleton, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddAny_OS(const std::string &p_argument_name)			{ return AddArg(kValueMaskAny | kValueMaskOptSingleton, p_argument_name); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_N(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogical | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_N(const std::string &p_argument_name)			{ return AddArg(kValueMaskInt | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_N(const std::string &p_argument_name)			{ return AddArg(kValueMaskFloat | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString_N(const std::string &p_argument_name)			{ return AddArg(kValueMaskString | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_N(const std::string &p_argument_name)			{ return AddArg(kValueMaskObject | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_N(const std::string &p_argument_name)		{ return AddArg(kValueMaskNumeric | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_N(const std::string &p_argument_name)	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskNULL, p_argument_name); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_ON(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogical | kValueMaskOptional | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_ON(const std::string &p_argument_name)			{ return AddArg(kValueMaskInt | kValueMaskOptional | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_ON(const std::string &p_argument_name)			{ return AddArg(kValueMaskFloat | kValueMaskOptional | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString_ON(const std::string &p_argument_name)		{ return AddArg(kValueMaskString | kValueMaskOptional | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_ON(const std::string &p_argument_name)		{ return AddArg(kValueMaskObject | kValueMaskOptional | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_ON(const std::string &p_argument_name)		{ return AddArg(kValueMaskNumeric | kValueMaskOptional | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_ON(const std::string &p_argument_name)	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptional | kValueMaskNULL, p_argument_name); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_SN(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogical | kValueMaskSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_SN(const std::string &p_argument_name)			{ return AddArg(kValueMaskInt | kValueMaskSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_SN(const std::string &p_argument_name)			{ return AddArg(kValueMaskFloat | kValueMaskSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString_SN(const std::string &p_argument_name)		{ return AddArg(kValueMaskString | kValueMaskSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_SN(const std::string &p_argument_name)		{ return AddArg(kValueMaskObject | kValueMaskSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_SN(const std::string &p_argument_name)		{ return AddArg(kValueMaskNumeric | kValueMaskSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_SN(const std::string &p_argument_name)	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskSingleton | kValueMaskNULL, p_argument_name); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_OSN(const std::string &p_argument_name)		{ return AddArg(kValueMaskLogical | kValueMaskOptSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_OSN(const std::string &p_argument_name)			{ return AddArg(kValueMaskInt | kValueMaskOptSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_OSN(const std::string &p_argument_name)		{ return AddArg(kValueMaskFloat | kValueMaskOptSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddString_OSN(const std::string &p_argument_name)		{ return AddArg(kValueMaskString | kValueMaskOptSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_OSN(const std::string &p_argument_name)		{ return AddArg(kValueMaskObject | kValueMaskOptSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_OSN(const std::string &p_argument_name)		{ return AddArg(kValueMaskNumeric | kValueMaskOptSingleton | kValueMaskNULL, p_argument_name); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_OSN(const std::string &p_argument_name)	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptSingleton | kValueMaskNULL, p_argument_name); }

void EidosFunctionSignature::CheckArguments(const string &p_call_type, EidosValue *const *const p_arguments, int p_argument_count) const
{
	// Check the number of arguments supplied
	if (!has_ellipsis_)
	{
		if (p_argument_count > arg_masks_.size())
			EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::CheckArguments): " << p_call_type << " " << function_name_ << "() requires at most " << arg_masks_.size() << " argument(s), but " << p_argument_count << " are supplied." << eidos_terminate();
	}
	
	// Check the types of all arguments specified in the signature
	for (int arg_index = 0; arg_index < arg_masks_.size(); ++arg_index)
	{
		EidosValueMask type_mask = arg_masks_[arg_index];
		bool is_optional = !!(type_mask & kValueMaskOptional);
		bool requires_singleton = !!(type_mask & kValueMaskSingleton);
		
		type_mask &= kValueMaskFlagStrip;
		
		// if no argument was passed for this slot, it needs to be an optional slot
		if (p_argument_count <= arg_index)
		{
			if (is_optional)
				break;			// all the rest of the arguments must be optional, so we're done checking
			else
				EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::CheckArguments): missing required argument for " << p_call_type << " " << function_name_ << "()." << eidos_terminate();
		}
		
		// an argument was passed, so check its type
		EidosValue *argument = p_arguments[arg_index];
		EidosValueType arg_type = argument->Type();
		
		if (type_mask != kValueMaskAny)
		{
			bool type_ok = true;
			
			switch (arg_type)
			{
				case EidosValueType::kValueNULL:		type_ok = !!(type_mask & kValueMaskNULL);		break;
				case EidosValueType::kValueLogical:	type_ok = !!(type_mask & kValueMaskLogical);	break;
				case EidosValueType::kValueString:		type_ok = !!(type_mask & kValueMaskString);	break;
				case EidosValueType::kValueInt:		type_ok = !!(type_mask & kValueMaskInt);		break;
				case EidosValueType::kValueFloat:		type_ok = !!(type_mask & kValueMaskFloat);	break;
				case EidosValueType::kValueObject:		type_ok = !!(type_mask & kValueMaskObject);	break;
			}
			
			if (!type_ok)
				EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::CheckArguments): argument " << arg_index + 1 << " cannot be type " << arg_type << " for " << p_call_type << " " << function_name_ << "()." << eidos_terminate();
			
			if (requires_singleton && (argument->Count() != 1))
				EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::CheckArguments): argument " << arg_index + 1 << " must be a singleton (size() == 1) for " << p_call_type << " " << function_name_ << "(), but size() == " << argument->Count() << "." << eidos_terminate();
		}
	}
}

void EidosFunctionSignature::CheckReturn(const string &p_call_type, EidosValue *p_result) const
{
	uint32_t retmask = return_mask_;
	bool return_type_ok = true;
	
	switch (p_result->Type())
	{
		case EidosValueType::kValueNULL:
			// A return type of NULL is always allowed, in fact; we don't want to have to specify this in the return type
			// This is a little fishy, but since NULL is used to indicate error conditions, NULL returns are exceptional,
			// and the return type indicates the type ordinarily returned in non-exceptional cases.  We just return here,
			// since we also don't want to do the singleton check below (since it would raise too).
			return;
		case EidosValueType::kValueLogical:	return_type_ok = !!(retmask & kValueMaskLogical);	break;
		case EidosValueType::kValueInt:		return_type_ok = !!(retmask & kValueMaskInt);		break;
		case EidosValueType::kValueFloat:		return_type_ok = !!(retmask & kValueMaskFloat);		break;
		case EidosValueType::kValueString:		return_type_ok = !!(retmask & kValueMaskString);	break;
		case EidosValueType::kValueObject:		return_type_ok = !!(retmask & kValueMaskObject);		break;
	}
	
	if (!return_type_ok)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::CheckReturn): internal error: return value cannot be type " << p_result->Type() << " for " << p_call_type << " " << function_name_ << "()." << eidos_terminate();
	
	bool return_is_singleton = !!(retmask & kValueMaskSingleton);
	
	if (return_is_singleton && (p_result->Count() != 1))
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::CheckReturn): internal error: return value must be a singleton (size() == 1) for " << p_call_type << " " << function_name_ << "(), but size() == " << p_result->Count() << eidos_terminate();
}

ostream &operator<<(ostream &p_outstream, const EidosFunctionSignature &p_signature)
{
	if (p_signature.is_class_method)
		p_outstream << "+ ";
	else if (p_signature.is_instance_method)
		p_outstream << "- ";
	
	p_outstream << "(" << StringForEidosValueMask(p_signature.return_mask_, "") << ")" << p_signature.function_name_ << "(";
	
	int arg_mask_count = (int)p_signature.arg_masks_.size();
	
	if (arg_mask_count == 0)
	{
		if (!p_signature.has_ellipsis_)
			p_outstream << gStr_void;
	}
	else
	{
		for (int arg_index = 0; arg_index < arg_mask_count; ++arg_index)
		{
			EidosValueMask type_mask = p_signature.arg_masks_[arg_index];
			const string &arg_name = p_signature.arg_names_[arg_index];
			
			if (arg_index > 0)
				p_outstream << ", ";
			
			p_outstream << StringForEidosValueMask(type_mask, arg_name);
		}
	}
	
	if (p_signature.has_ellipsis_)
		p_outstream << ((arg_mask_count > 0) ? ", ..." : "...");
	
	p_outstream << ")";
	
	// if the function is provided by a delegate, show the delegate's name
	if (p_signature.delegate_name_.length())
		p_outstream << " <" << p_signature.delegate_name_ << ">";
	
	return p_outstream;
}

bool CompareEidosFunctionSignatures(const EidosFunctionSignature *i, const EidosFunctionSignature *j)
{
	return (i->function_name_ < j->function_name_);
}



































































