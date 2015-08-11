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


#include "eidos_call_signature.h"


using std::string;
using std::vector;
using std::endl;
using std::ostream;


//
//	EidosCallSignature
//
#pragma mark EidosCallSignature

EidosCallSignature::EidosCallSignature(const string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask)
	: function_name_(p_function_name), function_id_(p_function_id), return_mask_(p_return_mask), return_class_(nullptr)
{
}

EidosCallSignature::EidosCallSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class)
	: function_name_(p_function_name), function_id_(p_function_id), return_mask_(p_return_mask), return_class_(p_return_class)
{
}

EidosCallSignature::~EidosCallSignature(void)
{
}

EidosCallSignature *EidosCallSignature::AddArg(EidosValueMask p_arg_mask, const std::string &p_argument_name, const EidosObjectClass *p_argument_class)
{
	bool is_optional = !!(p_arg_mask & kEidosValueMaskOptional);
	
	if (has_optional_args_ && !is_optional)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArg): cannot add a required argument after an optional argument has been added." << eidos_terminate();
	
	if (has_ellipsis_)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArg): cannot add an argument after an ellipsis." << eidos_terminate();
	
	if (p_argument_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArg): an argument name is required." << eidos_terminate();
	
	if (p_argument_class && !(p_arg_mask & kEidosValueMaskObject))
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArg): an object element type may only be supplied for an argument of object type." << eidos_terminate();
	
	arg_masks_.push_back(p_arg_mask);
	arg_names_.push_back(p_argument_name);
	arg_classes_.push_back(p_argument_class);
	
	if (is_optional)
		has_optional_args_ = true;
	
	return this;
}

EidosCallSignature *EidosCallSignature::AddEllipsis()
{
	if (has_optional_args_)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddEllipsis): cannot add an ellipsis after an optional argument has been added." << eidos_terminate();
	
	if (has_ellipsis_)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddEllipsis): cannot add more than one ellipsis." << eidos_terminate();
	
	has_ellipsis_ = true;
	
	return this;
}

EidosCallSignature *EidosCallSignature::AddLogical(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskLogical, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt(const std::string &p_argument_name)				{ return AddArg(kEidosValueMaskInt, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskString, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskNumeric, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogicalEquiv, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAnyBase(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskAnyBase, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAny(const std::string &p_argument_name)				{ return AddArg(kEidosValueMaskAny, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntObject(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)			{ return AddArg(kEidosValueMaskObject, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_O(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_O(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_O(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_O(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_O(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskString | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_O(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_O(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAnyBase_O(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskAnyBase | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAny_O(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskAny | kEidosValueMaskOptional, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntObject_O(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskOptional, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject_O(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)			{ return AddArg(kEidosValueMaskObject | kEidosValueMaskOptional, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskString | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_S(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAnyBase_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskAnyBase | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAny_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskAny | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntObject_S(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskSingleton, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject_S(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)			{ return AddArg(kEidosValueMaskObject | kEidosValueMaskSingleton, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_OS(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_OS(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_OS(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_OS(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_OS(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskString | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_OS(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_OS(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAnyBase_OS(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskAnyBase | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAny_OS(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskAny | kEidosValueMaskOptSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntObject_OS(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)	{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskOptSingleton, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject_OS(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)		{ return AddArg(kEidosValueMaskObject | kEidosValueMaskOptSingleton, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_N(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_N(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_N(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_N(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_N(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskString | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_N(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_N(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddObject_N(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)			{ return AddArg(kEidosValueMaskObject | kEidosValueMaskNULL, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_ON(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_ON(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_ON(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_ON(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_ON(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_ON(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_ON(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddObject_ON(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)		{ return AddArg(kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_SN(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_SN(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_SN(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddObject_SN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)		{ return AddArg(kEidosValueMaskObject | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_OSN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_OSN(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_OSN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_OSN(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_OSN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskString | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_OSN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_OSN(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddObject_OSN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class)		{ return AddArg(kEidosValueMaskObject | kEidosValueMaskOptSingleton | kEidosValueMaskNULL, p_argument_name, p_argument_class); }

void EidosCallSignature::CheckArguments(EidosValue *const *const p_arguments, int p_argument_count) const
{
	// Check the number of arguments supplied
	if (!has_ellipsis_)
	{
		if (p_argument_count > arg_masks_.size())
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArguments): " << CallType() << " " << function_name_ << "() requires at most " << arg_masks_.size() << " argument(s), but " << p_argument_count << " are supplied." << eidos_terminate();
	}
	
	// Check the types of all arguments specified in the signature
	for (int arg_index = 0; arg_index < arg_masks_.size(); ++arg_index)
	{
		EidosValueMask type_mask = arg_masks_[arg_index];
		bool is_optional = !!(type_mask & kEidosValueMaskOptional);
		bool requires_singleton = !!(type_mask & kEidosValueMaskSingleton);
		
		type_mask &= kEidosValueMaskFlagStrip;
		
		// if no argument was passed for this slot, it needs to be an optional slot
		if (p_argument_count <= arg_index)
		{
			if (is_optional)
				break;			// all the rest of the arguments must be optional, so we're done checking
			else
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArguments): missing required argument for " << CallType() << " " << function_name_ << "()." << eidos_terminate();
		}
		
		// an argument was passed, so check its type
		EidosValue *argument = p_arguments[arg_index];
		EidosValueType arg_type = argument->Type();
		
		if (type_mask != kEidosValueMaskAny)
		{
			bool type_ok = true;
			
			switch (arg_type)
			{
				case EidosValueType::kValueNULL:	type_ok = !!(type_mask & kEidosValueMaskNULL);		break;
				case EidosValueType::kValueLogical:	type_ok = !!(type_mask & kEidosValueMaskLogical);	break;
				case EidosValueType::kValueString:	type_ok = !!(type_mask & kEidosValueMaskString);		break;
				case EidosValueType::kValueInt:		type_ok = !!(type_mask & kEidosValueMaskInt);		break;
				case EidosValueType::kValueFloat:	type_ok = !!(type_mask & kEidosValueMaskFloat);		break;
				case EidosValueType::kValueObject:
					type_ok = !!(type_mask & kEidosValueMaskObject);
					
					// If the argument is object type, and is allowed to be object type, and an object element type was specified
					// in the signature, check the object element type of the argument.  Note this uses pointer equality!
					const EidosObjectClass *arg_obj_class = arg_classes_[arg_index];
					
					if (type_ok && arg_obj_class && (((EidosValue_Object *)argument)->Class() != arg_obj_class))
					{
						type_ok = false;
						EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArguments): argument " << arg_index + 1 << " cannot be object element type " << argument->ElementType() << " for " << CallType() << " " << function_name_ << "(); expected object element type " << arg_obj_class->ElementType() << "." << eidos_terminate();
					}
					break;
			}
			
			if (!type_ok)
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArguments): argument " << arg_index + 1 << " cannot be type " << arg_type << " for " << CallType() << " " << function_name_ << "()." << eidos_terminate();
			
			if (requires_singleton && (argument->Count() != 1))
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArguments): argument " << arg_index + 1 << " must be a singleton (size() == 1) for " << CallType() << " " << function_name_ << "(), but size() == " << argument->Count() << "." << eidos_terminate();
		}
	}
}

void EidosCallSignature::CheckReturn(EidosValue *p_result) const
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
		case EidosValueType::kValueLogical:	return_type_ok = !!(retmask & kEidosValueMaskLogical);	break;
		case EidosValueType::kValueInt:		return_type_ok = !!(retmask & kEidosValueMaskInt);		break;
		case EidosValueType::kValueFloat:	return_type_ok = !!(retmask & kEidosValueMaskFloat);		break;
		case EidosValueType::kValueString:	return_type_ok = !!(retmask & kEidosValueMaskString);	break;
		case EidosValueType::kValueObject:
			return_type_ok = !!(retmask & kEidosValueMaskObject);
			
			// If the return is object type, and is allowed to be object type, and an object element type was specified
			// in the signature, check the object element type of the return.  Note this uses pointer equality!
			if (return_type_ok && return_class_ && (((EidosValue_Object *)p_result)->Class() != return_class_))
			{
				return_type_ok = false;
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): internal error: object return value cannot be element type " << p_result->ElementType() << " for " << CallType() << " " << function_name_ << "(); expected object element type " << return_class_->ElementType() << "." << eidos_terminate();
			}
			break;
	}
	
	if (!return_type_ok)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): internal error: return value cannot be type " << p_result->Type() << " for " << CallType() << " " << function_name_ << "()." << eidos_terminate();
	
	bool return_is_singleton = !!(retmask & kEidosValueMaskSingleton);
	
	if (return_is_singleton && (p_result->Count() != 1))
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): internal error: return value must be a singleton (size() == 1) for " << CallType() << " " << function_name_ << "(), but size() == " << p_result->Count() << eidos_terminate();
}

std::string EidosCallSignature::CallDelegate(void) const
{
	return "";
}

ostream &operator<<(ostream &p_outstream, const EidosCallSignature &p_signature)
{
	//
	//	Note this logic is paralleled in -[EidosConsoleWindowController updateStatusLineWithSignature:].
	//	These two should be kept in synch so the user-visible format of signatures is consistent.
	//
	
	p_outstream << p_signature.CallPrefix();	// "", "- ", or "+ " depending on our subclass
	
	p_outstream << "(" << StringForEidosValueMask(p_signature.return_mask_, p_signature.return_class_, "");
	
	p_outstream << ")" << p_signature.function_name_ << "(";
	
	int arg_mask_count = (int)p_signature.arg_masks_.size();
	
	if (arg_mask_count == 0)
	{
		if (!p_signature.has_ellipsis_)
			p_outstream << gEidosStr_void;
	}
	else
	{
		for (int arg_index = 0; arg_index < arg_mask_count; ++arg_index)
		{
			EidosValueMask type_mask = p_signature.arg_masks_[arg_index];
			const string &arg_name = p_signature.arg_names_[arg_index];
			const EidosObjectClass *arg_obj_class = p_signature.arg_classes_[arg_index];
			
			if (arg_index > 0)
				p_outstream << ", ";
			
			p_outstream << StringForEidosValueMask(type_mask, arg_obj_class, arg_name);
		}
	}
	
	if (p_signature.has_ellipsis_)
		p_outstream << ((arg_mask_count > 0) ? ", ..." : "...");
	
	p_outstream << ")";
	
	// if the function is provided by a delegate, show the delegate's name
	p_outstream << p_signature.CallDelegate();
	
	return p_outstream;
}

bool CompareEidosCallSignatures(const EidosCallSignature *i, const EidosCallSignature *j)
{
	return (i->function_name_ < j->function_name_);
}


//
//	EidosFunctionSignature
//
#pragma mark -
#pragma mark EidosFunctionSignature

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask)
	: EidosCallSignature(p_function_name, p_function_id, p_return_mask)
{
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class)
	: EidosCallSignature(p_function_name, p_function_id, p_return_mask, p_return_class)
{
}

EidosFunctionSignature::EidosFunctionSignature(const string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, EidosDelegateFunctionPtr p_delegate_function, void *p_delegate_object, const string &p_delegate_name)
	: EidosCallSignature(p_function_name, p_function_id, p_return_mask), delegate_function_(p_delegate_function), delegate_object_(p_delegate_object), delegate_name_(p_delegate_name)
{
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class, EidosDelegateFunctionPtr p_delegate_function, void *p_delegate_object, const std::string &p_delegate_name)
	: EidosCallSignature(p_function_name, p_function_id, p_return_mask, p_return_class), delegate_function_(p_delegate_function), delegate_object_(p_delegate_object), delegate_name_(p_delegate_name)
{
}

EidosFunctionSignature::~EidosFunctionSignature(void)
{
}

std::string EidosFunctionSignature::CallType(void) const
{
	return "function";
}

std::string EidosFunctionSignature::CallPrefix(void) const
{
	return "";
}

std::string EidosFunctionSignature::CallDelegate(void) const
{
	if (delegate_name_.length())
	{
		std::string delegate_tag;
		
		delegate_tag += " <";
		delegate_tag += delegate_name_;
		delegate_tag += ">";
		
		return delegate_tag;
	}
	
	return "";
}


//
//	EidosMethodSignature
//
#pragma mark -
#pragma mark EidosMethodSignature

EidosMethodSignature::EidosMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, bool p_is_class_method)
	: EidosCallSignature(p_function_name, EidosFunctionIdentifier::kNoFunction, p_return_mask), is_class_method(p_is_class_method)
{
}

EidosMethodSignature::EidosMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class, bool p_is_class_method)
	: EidosCallSignature(p_function_name, EidosFunctionIdentifier::kNoFunction, p_return_mask, p_return_class), is_class_method(p_is_class_method)
{
}

std::string EidosMethodSignature::CallType(void) const
{
	return "method";
}

EidosMethodSignature::~EidosMethodSignature(void)
{
}


//
//	EidosInstanceMethodSignature
//
#pragma mark -
#pragma mark EidosInstanceMethodSignature

EidosInstanceMethodSignature::EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask)
	: EidosMethodSignature(p_function_name, p_return_mask, false)
{
}

EidosInstanceMethodSignature::EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class)
	: EidosMethodSignature(p_function_name, p_return_mask, p_return_class, false)
{
}

std::string EidosInstanceMethodSignature::CallPrefix(void) const
{
	return "- ";
}

EidosInstanceMethodSignature::~EidosInstanceMethodSignature(void)
{
}


//
//	EidosClassMethodSignature
//
#pragma mark -
#pragma mark EidosClassMethodSignature

EidosClassMethodSignature::EidosClassMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask)
	: EidosMethodSignature(p_function_name, p_return_mask, true)
{
}

EidosClassMethodSignature::EidosClassMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class)
	: EidosMethodSignature(p_function_name, p_return_mask, p_return_class, true)
{
}

std::string EidosClassMethodSignature::CallPrefix(void) const
{
	return "+ ";
}

EidosClassMethodSignature::~EidosClassMethodSignature(void)
{
}


































































