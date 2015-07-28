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

EidosFunctionSignature *EidosFunctionSignature::AddArg(EidosValueMask p_arg_mask)
{
	bool is_optional = !!(p_arg_mask & kValueMaskOptional);
	
	if (has_optional_args_ && !is_optional)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::AddArg): cannot add a required argument after an optional argument has been added." << eidos_terminate();
	
	if (has_ellipsis_)
		EIDOS_TERMINATION << "ERROR (EidosFunctionSignature::AddArg): cannot add an argument after an ellipsis." << eidos_terminate();
	
	arg_masks_.push_back(p_arg_mask);
	
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

EidosFunctionSignature *EidosFunctionSignature::AddLogical()			{ return AddArg(kValueMaskLogical); }
EidosFunctionSignature *EidosFunctionSignature::AddInt()				{ return AddArg(kValueMaskInt); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat()			{ return AddArg(kValueMaskFloat); }
EidosFunctionSignature *EidosFunctionSignature::AddString()			{ return AddArg(kValueMaskString); }
EidosFunctionSignature *EidosFunctionSignature::AddObject()			{ return AddArg(kValueMaskObject); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric()			{ return AddArg(kValueMaskNumeric); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv()		{ return AddArg(kValueMaskLogicalEquiv); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase()			{ return AddArg(kValueMaskAnyBase); }
EidosFunctionSignature *EidosFunctionSignature::AddAny()				{ return AddArg(kValueMaskAny); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_O()		{ return AddArg(kValueMaskLogical | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_O()			{ return AddArg(kValueMaskInt | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_O()			{ return AddArg(kValueMaskFloat | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddString_O()			{ return AddArg(kValueMaskString | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_O()			{ return AddArg(kValueMaskObject | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_O()		{ return AddArg(kValueMaskNumeric | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_O()	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase_O()		{ return AddArg(kValueMaskAnyBase | kValueMaskOptional); }
EidosFunctionSignature *EidosFunctionSignature::AddAny_O()			{ return AddArg(kValueMaskAny | kValueMaskOptional); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_S()		{ return AddArg(kValueMaskLogical | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_S()			{ return AddArg(kValueMaskInt | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_S()			{ return AddArg(kValueMaskFloat | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddString_S()			{ return AddArg(kValueMaskString | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_S()			{ return AddArg(kValueMaskObject | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_S()		{ return AddArg(kValueMaskNumeric | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_S()	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase_S()		{ return AddArg(kValueMaskAnyBase | kValueMaskSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddAny_S()			{ return AddArg(kValueMaskAny | kValueMaskSingleton); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_OS()		{ return AddArg(kValueMaskLogical | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_OS()			{ return AddArg(kValueMaskInt | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_OS()			{ return AddArg(kValueMaskFloat | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddString_OS()		{ return AddArg(kValueMaskString | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_OS()		{ return AddArg(kValueMaskObject | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_OS()		{ return AddArg(kValueMaskNumeric | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_OS()	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddAnyBase_OS()		{ return AddArg(kValueMaskAnyBase | kValueMaskOptSingleton); }
EidosFunctionSignature *EidosFunctionSignature::AddAny_OS()			{ return AddArg(kValueMaskAny | kValueMaskOptSingleton); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_N()		{ return AddArg(kValueMaskLogical | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_N()			{ return AddArg(kValueMaskInt | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_N()			{ return AddArg(kValueMaskFloat | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddString_N()			{ return AddArg(kValueMaskString | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_N()			{ return AddArg(kValueMaskObject | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_N()		{ return AddArg(kValueMaskNumeric | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_N()	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskNULL); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_ON()		{ return AddArg(kValueMaskLogical | kValueMaskOptional | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_ON()			{ return AddArg(kValueMaskInt | kValueMaskOptional | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_ON()			{ return AddArg(kValueMaskFloat | kValueMaskOptional | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddString_ON()		{ return AddArg(kValueMaskString | kValueMaskOptional | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_ON()		{ return AddArg(kValueMaskObject | kValueMaskOptional | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_ON()		{ return AddArg(kValueMaskNumeric | kValueMaskOptional | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_ON()	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptional | kValueMaskNULL); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_SN()		{ return AddArg(kValueMaskLogical | kValueMaskSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_SN()			{ return AddArg(kValueMaskInt | kValueMaskSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_SN()			{ return AddArg(kValueMaskFloat | kValueMaskSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddString_SN()		{ return AddArg(kValueMaskString | kValueMaskSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_SN()		{ return AddArg(kValueMaskObject | kValueMaskSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_SN()		{ return AddArg(kValueMaskNumeric | kValueMaskSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_SN()	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskSingleton | kValueMaskNULL); }

EidosFunctionSignature *EidosFunctionSignature::AddLogical_OSN()		{ return AddArg(kValueMaskLogical | kValueMaskOptSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddInt_OSN()			{ return AddArg(kValueMaskInt | kValueMaskOptSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddFloat_OSN()		{ return AddArg(kValueMaskFloat | kValueMaskOptSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddString_OSN()		{ return AddArg(kValueMaskString | kValueMaskOptSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddObject_OSN()		{ return AddArg(kValueMaskObject | kValueMaskOptSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddNumeric_OSN()		{ return AddArg(kValueMaskNumeric | kValueMaskOptSingleton | kValueMaskNULL); }
EidosFunctionSignature *EidosFunctionSignature::AddLogicalEquiv_OSN()	{ return AddArg(kValueMaskLogicalEquiv | kValueMaskOptSingleton | kValueMaskNULL); }

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
	
	p_outstream << "(" << StringForEidosValueMask(p_signature.return_mask_) << ")" << p_signature.function_name_ << "(";
	
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
			
			if (arg_index > 0)
				p_outstream << ", ";
			
			p_outstream << StringForEidosValueMask(type_mask);
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



































































