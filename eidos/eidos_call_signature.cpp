//
//  eidos_function_signature.cpp
//  Eidos
//
//  Created by Ben Haller on 5/16/15.
//  Copyright (c) 2015-2025 Benjamin C. Haller.  All rights reserved.
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


#include "eidos_call_signature.h"

#include <string>
#include <utility>
#include <vector>

#include "eidos_value.h"


//
//	EidosCallSignature
//
#pragma mark -
#pragma mark EidosCallSignature
#pragma mark -

EidosCallSignature::EidosCallSignature(const std::string &p_call_name, EidosValueMask p_return_mask)
	: call_name_(p_call_name), call_id_(EidosStringRegistry::GlobalStringIDForString(p_call_name)), return_mask_(p_return_mask), return_class_(nullptr)
{
}

EidosCallSignature::EidosCallSignature(const std::string &p_call_name, EidosValueMask p_return_mask, const EidosClass *p_return_class)
	: call_name_(p_call_name), call_id_(EidosStringRegistry::GlobalStringIDForString(p_call_name)), return_mask_(p_return_mask), return_class_(p_return_class)
{
}

EidosCallSignature::~EidosCallSignature(void)
{
}

EidosCallSignature *EidosCallSignature::AddArg(EidosValueMask p_arg_mask, const std::string &p_argument_name, const EidosClass *p_argument_class)
{
	return AddArgWithDefault(p_arg_mask, p_argument_name, p_argument_class, EidosValue_SP(nullptr));
}

//NOLINTNEXTLINE : clang-tidy doesn't like that p_default_value gets copied instead of passed by reference, but I prefer this
EidosCallSignature *EidosCallSignature::AddArgWithDefault(EidosValueMask p_arg_mask, const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value, bool p_fault_tolerant)
{
	bool is_optional = !!(p_arg_mask & kEidosValueMaskOptional);
	
	// If we're doing a fault-tolerant parse and the signature is badly malformed, we just don't add the offending argument
	if (has_optional_args_ && !is_optional)
	{
		if (p_fault_tolerant)
			return this;
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) cannot add a required argument after an optional argument has been added." << EidosTerminate(nullptr);
	}
	if (p_argument_name.size() == 0)
	{
		if (p_fault_tolerant)
			return this;
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) an argument name is required." << EidosTerminate(nullptr);
	}
	if (p_argument_class && !(p_arg_mask & kEidosValueMaskObject))
	{
		if (p_fault_tolerant)
			return this;
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) an object element type may only be supplied for an argument of object type." << EidosTerminate(nullptr);
	}
	
	// Default values should be marked constant, just to be safe.  We just make a copy here;
	// it's not worth trying to avoid that, since this is just startup overhead.
	// BCH: Note that copying removes the invisible flag that we use internally in some spots;
	// we therefore note here whether the original value was invisible NULL, for reference below.
	bool default_value_is_invisible_NULL = (p_default_value.get() == gStaticEidosValueNULLInvisible);
	
	if (p_default_value)
	{
		p_default_value = p_default_value->CopyValues();
		p_default_value->MarkAsConstant();
	}
	
	arg_masks_.emplace_back(p_arg_mask);
	arg_names_.emplace_back(p_argument_name);
	arg_name_IDs_.emplace_back(EidosStringRegistry::GlobalStringIDForString(p_argument_name));
	arg_classes_.emplace_back(p_argument_class);
	arg_defaults_.emplace_back(p_default_value);
	
	if (is_optional)
		has_optional_args_ = true;
	
	// If we're doing a fault-tolerant parse, skip the rest; we're not going to raise anyway, so there's no point in checking
	if (p_fault_tolerant)
		return this;
	
	// Check the default argument; see CheckArguments() for parallel code
	if (is_optional && !p_default_value)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) no default argument supplied for optional argument." << EidosTerminate(nullptr);
	if (!is_optional && p_default_value)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) default argument supplied for required argument." << EidosTerminate(nullptr);
	
	if (is_optional)
	{
		EidosValueMask type_mask = p_arg_mask;
		bool requires_singleton = !!(type_mask & kEidosValueMaskSingleton);
		
		type_mask &= kEidosValueMaskFlagStrip;
		
		// an argument was passed, so check its type
		EidosValue *argument = p_default_value.get();
		EidosValueType arg_type = argument->Type();
		
		if ((type_mask != kEidosValueMaskAny) && !default_value_is_invisible_NULL)	// allow gStaticEidosValueNULLInvisible as a default even if the argument is not labelled as taking NULL; this is for internal use only
		{
			bool type_ok = true;
			
			switch (arg_type)
			{
				case EidosValueType::kValueVOID:	type_ok = false;									break;
				case EidosValueType::kValueNULL:	type_ok = !!(type_mask & kEidosValueMaskNULL);		break;
				case EidosValueType::kValueLogical:	type_ok = !!(type_mask & kEidosValueMaskLogical);	break;
				case EidosValueType::kValueString:	type_ok = !!(type_mask & kEidosValueMaskString);	break;
				case EidosValueType::kValueInt:		type_ok = !!(type_mask & kEidosValueMaskInt);		break;
				case EidosValueType::kValueFloat:	type_ok = !!(type_mask & kEidosValueMaskFloat);		break;
				case EidosValueType::kValueObject:
					type_ok = !!(type_mask & kEidosValueMaskObject);
					
					// If the argument is object type, and is allowed to be object type, and an object element type was specified
					// in the signature, check the object element type of the argument.  Note this uses pointer equality!
					const EidosClass *signature_class = p_argument_class;
					
					if (type_ok && signature_class)
					{
						const EidosClass *argument_class = ((EidosValue_Object *)argument)->Class();
						
						if (argument_class != signature_class)
						{
							// Empty object vectors of undefined class are allowed to be passed for type-specified parameters; such vectors are generic
							if ((argument_class == gEidosObject_Class) && (argument->Count() == 0))
								break;
							
							EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) default argument cannot be object element type " << argument->ElementType() << "; expected object element type " << signature_class->ClassNameForDisplay() << "." << EidosTerminate(nullptr);
						}
					}
					break;
			}
			
			if (!type_ok)
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) default argument cannot be type " << arg_type << "." << EidosTerminate(nullptr);
			
			// if NULL is explicitly permitted by the signature, we skip the singleton check
			if (requires_singleton && (argument->Count() != 1) && (arg_type != EidosValueType::kValueNULL))
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddArgWithDefault): (internal error) default argument must be a singleton (size() == 1), but size() == " << argument->Count() << "." << EidosTerminate(nullptr);
		}
	}
	
	return this;
}

EidosCallSignature *EidosCallSignature::AddEllipsis(void)
{
	if (has_optional_args_)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddEllipsis): cannot add an ellipsis after an optional argument has been added." << EidosTerminate(nullptr);
	
	if (has_ellipsis_)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::AddEllipsis): cannot add more than one ellipsis." << EidosTerminate(nullptr);
	
	arg_masks_.emplace_back(kEidosValueMaskAny);
	arg_names_.emplace_back(gEidosStr_ELLIPSIS);
	arg_name_IDs_.emplace_back(gEidosID_ELLIPSIS);
	arg_classes_.emplace_back(nullptr);
	arg_defaults_.emplace_back(nullptr);
	
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
EidosCallSignature *EidosCallSignature::AddIntObject(const std::string &p_argument_name, const EidosClass *p_argument_class)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject(const std::string &p_argument_name, const EidosClass *p_argument_class)			{ return AddArg(kEidosValueMaskObject, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_O(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddInt_O(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddFloat_O(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskFloat | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntString_O(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddString_O(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskString | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddNumeric_O(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskNumeric | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_O(const std::string &p_argument_name, EidosValue_SP p_default_value)	{ return AddArgWithDefault(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddAnyBase_O(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskAnyBase | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddAny_O(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskAny | kEidosValueMaskOptional, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntObject_O(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskOptional, p_argument_name, p_argument_class, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddObject_O(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskObject | kEidosValueMaskOptional, p_argument_name, p_argument_class, std::move(p_default_value)); }

EidosCallSignature *EidosCallSignature::AddLogical_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskString | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_S(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAnyBase_S(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskAnyBase | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddAny_S(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskAny | kEidosValueMaskSingleton, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntObject_S(const std::string &p_argument_name, const EidosClass *p_argument_class)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskSingleton, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject_S(const std::string &p_argument_name, const EidosClass *p_argument_class)			{ return AddArg(kEidosValueMaskObject | kEidosValueMaskSingleton, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddInt_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddFloat_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskFloat | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntString_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddString_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddNumeric_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskNumeric | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)	{ return AddArgWithDefault(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddAnyBase_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskAnyBase | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddAny_OS(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskAny | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntObject_OS(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)	{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, p_argument_class, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddObject_OS(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, p_argument_name, p_argument_class, std::move(p_default_value)); }

EidosCallSignature *EidosCallSignature::AddLogical_N(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_N(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_N(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_N(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_N(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskString | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_N(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_N(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntObject_N(const std::string &p_argument_name, const EidosClass *p_argument_class)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskNULL, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject_N(const std::string &p_argument_name, const EidosClass *p_argument_class)			{ return AddArg(kEidosValueMaskObject | kEidosValueMaskNULL, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_ON(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddInt_ON(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddFloat_ON(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskFloat | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntString_ON(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddString_ON(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddNumeric_ON(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskNumeric | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_ON(const std::string &p_argument_name, EidosValue_SP p_default_value)	{ return AddArgWithDefault(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntObject_ON(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, p_argument_class, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddObject_ON(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskNULL, p_argument_name, p_argument_class, std::move(p_default_value)); }

EidosCallSignature *EidosCallSignature::AddLogical_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskLogical | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddInt_SN(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskInt | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddFloat_SN(const std::string &p_argument_name)			{ return AddArg(kEidosValueMaskFloat | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntString_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddString_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddNumeric_SN(const std::string &p_argument_name)		{ return AddArg(kEidosValueMaskNumeric | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_SN(const std::string &p_argument_name)	{ return AddArg(kEidosValueMaskLogicalEquiv | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr); }
EidosCallSignature *EidosCallSignature::AddIntObject_SN(const std::string &p_argument_name, const EidosClass *p_argument_class)		{ return AddArg(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, p_argument_class); }
EidosCallSignature *EidosCallSignature::AddObject_SN(const std::string &p_argument_name, const EidosClass *p_argument_class)		{ return AddArg(kEidosValueMaskObject | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, p_argument_class); }

EidosCallSignature *EidosCallSignature::AddLogical_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddInt_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value)			{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddFloat_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskFloat | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntString_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value)	{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddString_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskString | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddNumeric_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskNumeric | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddLogicalEquiv_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value)	{ return AddArgWithDefault(kEidosValueMaskLogicalEquiv | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, nullptr, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddIntObject_OSN(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskInt | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, p_argument_class, std::move(p_default_value)); }
EidosCallSignature *EidosCallSignature::AddObject_OSN(const std::string &p_argument_name, const EidosClass *p_argument_class, EidosValue_SP p_default_value)		{ return AddArgWithDefault(kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton | kEidosValueMaskNULL, p_argument_name, p_argument_class, std::move(p_default_value)); }

EidosCallSignature *EidosCallSignature::MarkDeprecated(void)
{
	// At present, the only consequence of deprecation is that the property/method is not listed in the documentation
	deprecated_ = true;
	return this;
}

void EidosCallSignature::CheckArgument(EidosValue *p_argument, int p_signature_index) const
{
	EidosValueType arg_type = p_argument->Type();
	
	if (has_ellipsis_ && (arg_name_IDs_[p_signature_index] == gEidosID_ELLIPSIS))
	{
		// If we're checking against the ellipsis argument, the only rule is that it can't be void
		if (arg_type == EidosValueType::kValueVOID)
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArgument): argument " << p_signature_index + 1 << " (" << arg_names_[p_signature_index] << ") cannot be type " << arg_type << " for " << CallType() << " " << call_name_ << "()." << EidosTerminate(nullptr);
	}
	else
	{
		EidosValueMask type_mask_unstripped = arg_masks_[p_signature_index];
		EidosValueMask type_mask = (type_mask_unstripped & kEidosValueMaskFlagStrip);
		bool type_ok;
		
		switch (arg_type)
		{
			case EidosValueType::kValueVOID:	type_ok = false; break;		// never legal regardless of type_mask; void may never be passed as a parameter
			case EidosValueType::kValueNULL:
			{
				if (type_mask & kEidosValueMaskNULL)
					return;
				
				type_ok = false;
				break;
			}
			case EidosValueType::kValueLogical:	type_ok = !!(type_mask & kEidosValueMaskLogical);	break;
			case EidosValueType::kValueString:	type_ok = !!(type_mask & kEidosValueMaskString);	break;
			case EidosValueType::kValueInt:		type_ok = !!(type_mask & kEidosValueMaskInt);		break;
			case EidosValueType::kValueFloat:	type_ok = !!(type_mask & kEidosValueMaskFloat);		break;
			case EidosValueType::kValueObject:
			{
				type_ok = !!(type_mask & kEidosValueMaskObject);
				
				// If the argument is object type, and is allowed to be object type, and an object element type was specified
				// in the signature, check the object element type of the argument.  Note this uses pointer equality!
				const EidosClass *signature_class = arg_classes_[p_signature_index];
				
				if (type_ok && signature_class)
				{
					const EidosClass *argument_class = ((EidosValue_Object *)p_argument)->Class();
					
					if (argument_class != signature_class)
					{
						// Empty object vectors of undefined class are allowed to be passed for type-specified parameters; such vectors are generic
						if ((argument_class == gEidosObject_Class) && (p_argument->Count() == 0))
							break;
						
						if (!argument_class->IsSubclassOfClass(signature_class))
							EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArgument): argument " << p_signature_index + 1 << " cannot be object element type " << p_argument->ElementType() << " for " << CallType() << " " << call_name_ << "(); expected object element type " << signature_class->ClassNameForDisplay() << "." << EidosTerminate(nullptr);
					}
				}
				break;
			}
			default: type_ok = false; break;	// never hit, here to make the Ubuntu compiler happy
		}
		
		if (!type_ok)
		{
			// We have special error-handling for apply() because sapply() used to be named apply() and we want to steer users to the new call
			if ((call_name_ == "apply") && (arg_type == EidosValueType::kValueString))
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArgument): argument " << p_signature_index + 1 << " (" << arg_names_[p_signature_index] << ") cannot be type " << arg_type << " for " << CallType() << " " << call_name_ << "()." << std::endl << "NOTE: The apply() function was renamed sapply() in Eidos 1.6, and a new function named apply() has been added; you may need to change this call to be a call to sapply() instead." << EidosTerminate(nullptr);
			
			// Special error-handling for defineSpatialMap() because its gridSize parameter was removed in SLiM 3.5
			if ((call_name_ == "defineSpatialMap") &&
				(((p_signature_index == 2) && (arg_type == EidosValueType::kValueNULL)) ||
				 ((p_signature_index == 3) && ((arg_type == EidosValueType::kValueFloat) || (arg_type == EidosValueType::kValueInt))) ||
				 ((p_signature_index == 4) && (arg_type == EidosValueType::kValueLogical))))
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArgument): argument " << p_signature_index + 1 << " (" << arg_names_[p_signature_index] << ") cannot be type " << arg_type << " for " << CallType() << " " << call_name_ << "()." << std::endl << "NOTE: The defineSpatialMap() method was changed in SLiM 3.5, breaking backward compatibility.  Please see the manual for guidance on updating your code." << EidosTerminate(nullptr);
			
			// Special error-handling for initializeSLiMOptions() because its mutationRuns parameter changed to doMutationRunExperiments, and changed from integer to logical
			if ((call_name_ == "initializeSLiMOptions") && (p_signature_index == 3) && (arg_type == EidosValueType::kValueInt))
				EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArgument): argument " << p_signature_index + 1 << " (" << arg_names_[p_signature_index] << ") cannot be type " << arg_type << " for " << CallType() << " " << call_name_ << "()." << std::endl << "NOTE: The mutationRuns parameter to initializeSLiMOptions() was changed in SLiM 5, breaking backward compatibility.  Please see the manual for guidance on updating your code." << EidosTerminate(nullptr);
			
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArgument): argument " << p_signature_index + 1 << " (" << arg_names_[p_signature_index] << ") cannot be type " << arg_type << " for " << CallType() << " " << call_name_ << "()." << EidosTerminate(nullptr);
		}
		
		// if the argument is NULL, this singleton check is skipped by the continue statement above
		if ((type_mask_unstripped & kEidosValueMaskSingleton) && (p_argument->Count() != 1))
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArgument): argument " << p_signature_index + 1 << " (" << arg_names_[p_signature_index] << ") must be a singleton (size() == 1) for " << CallType() << " " << call_name_ << "(), but size() == " << p_argument->Count() << "." << EidosTerminate(nullptr);
	}
}

void EidosCallSignature::CheckArguments(const std::vector<EidosValue_SP> &p_arguments) const
{
	size_t argument_count = p_arguments.size();
	size_t arg_masks_size = arg_masks_.size();
	size_t minimum_arg_count = (has_ellipsis_ ? arg_masks_size - 1 : arg_masks_size);	// if there is an ellipsis, it is optional, but it occupies one argument slot
	
	// Check the number of arguments supplied; note that now our function dispatch code guarantees that every argument is present, including optional arguments
	if (!has_ellipsis_ && (argument_count > arg_masks_size))
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArguments): " << CallType() << " " << call_name_ << "() requires at most " << minimum_arg_count << " argument(s), but " << argument_count << " are supplied (after incorporating default arguments)." << EidosTerminate(nullptr);
	
	if (argument_count < minimum_arg_count)
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckArguments): " << CallType() << " " << call_name_ << "() requires " << minimum_arg_count << " argument(s), but " << argument_count << " are supplied (after incorporating default arguments)." << EidosTerminate(nullptr);
	
	// Check the types of all arguments specified in the signature
	int signature_index = 0;
	
	for (unsigned int arg_index = 0; arg_index < argument_count; ++arg_index, ++signature_index)
	{
		// If the current signature index is an ellipsis, deal with it completely here; just check the ellipsis arguments for void
		if (arg_name_IDs_[signature_index] == gEidosID_ELLIPSIS)
		{
			int first_ellipsis = arg_index;
			int last_ellipsis = first_ellipsis + (int)(argument_count - minimum_arg_count) - 1;
			
			for (int ellipsis_index = first_ellipsis; ellipsis_index <= last_ellipsis; ++ellipsis_index)
				CheckArgument(p_arguments[ellipsis_index].get(), signature_index);
			
			++signature_index;
			arg_index = last_ellipsis + 1;
			if (arg_index == argument_count)
				break;
		}
		
		CheckArgument(p_arguments[arg_index].get(), signature_index);
	}
}

void EidosCallSignature::CheckReturn(const EidosValue &p_result) const
{
	uint32_t retmask = return_mask_;
	bool return_type_ok = true;
	
	switch (p_result.Type())
	{
		case EidosValueType::kValueVOID:	return_type_ok = !!(retmask & kEidosValueMaskVOID);		break;
		case EidosValueType::kValueNULL:
		{
			// A return type of NULL is always allowed, in fact; we don't want to have to specify this in the return type
			// This is a little fishy, but since NULL is used to indicate error conditions, NULL returns are exceptional,
			// and the return type indicates the type ordinarily returned in non-exceptional cases.  We just return here,
			// since we also don't want to do the singleton check below (since it would raise too).
			
			// BCH 23 March 2018: We do not allow a return of NULL from functions that are declared as returning void.
			if (retmask == kEidosValueMaskVOID)
			{
				return_type_ok = false;
				break;
			}
			
			return;
		}
		case EidosValueType::kValueLogical:	return_type_ok = !!(retmask & kEidosValueMaskLogical);	break;
		case EidosValueType::kValueInt:		return_type_ok = !!(retmask & kEidosValueMaskInt);		break;
		case EidosValueType::kValueFloat:	return_type_ok = !!(retmask & kEidosValueMaskFloat);	break;
		case EidosValueType::kValueString:	return_type_ok = !!(retmask & kEidosValueMaskString);	break;
		case EidosValueType::kValueObject:
			return_type_ok = !!(retmask & kEidosValueMaskObject);
			
			// If the return is object type, and is allowed to be object type, and an object element type was specified
			// in the signature, check the object element type of the return.  Note this uses pointer equality!
			if (return_type_ok && return_class_ && (((EidosValue_Object &)p_result).Class() != return_class_))
			{
				if (!((EidosValue_Object &)p_result).Class()->IsSubclassOfClass(return_class_))
					EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): object return value cannot be element type " << p_result.ElementType() << " for " << CallType() << " " << call_name_ << "(); expected object element type " << return_class_->ClassNameForDisplay() << "." << EidosTerminate(nullptr);
			}
			break;
	}
	
	if (!return_type_ok)
	{
		// We try to emit more helpful error messages when void is involved in the type mismatch
		if (retmask == kEidosValueMaskVOID)
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): return value must be void for " << CallType() << " " << call_name_ << "(); use a \"return;\" statement if you wish to explicitly return with no return value." << EidosTerminate(nullptr);
		else if (p_result.Type() == EidosValueType::kValueVOID)
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): return value cannot be void for " << CallType() << " " << call_name_ << "(); use a \"return\" statement to explicitly return a value." << EidosTerminate(nullptr);
		else
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): return value cannot be type " << p_result.Type() << " for " << CallType() << " " << call_name_ << "()." << EidosTerminate(nullptr);
	}
	
	bool return_is_singleton = !!(retmask & kEidosValueMaskSingleton);
	
	if (return_is_singleton && (p_result.Count() != 1))
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckReturn): return value must be a singleton (size() == 1) for " << CallType() << " " << call_name_ << "(), but size() == " << p_result.Count() << "." << EidosTerminate(nullptr);
}

void EidosCallSignature::CheckAggregateReturn(const EidosValue &p_result, size_t p_expected_size) const
{
	uint32_t retmask = return_mask_;
	bool return_type_ok = true;
	
	switch (p_result.Type())
	{
		case EidosValueType::kValueVOID:	return_type_ok = !!(retmask & kEidosValueMaskVOID);		break;
		case EidosValueType::kValueNULL:
		{
			// A return type of NULL is always allowed, in fact; we don't want to have to specify this in the return type
			// This is a little fishy, but since NULL is used to indicate error conditions, NULL returns are exceptional,
			// and the return type indicates the type ordinarily returned in non-exceptional cases.  We just return here,
			// since we also don't want to do the singleton check below (since it would raise too).
			
			// BCH 23 March 2018: We do not allow a return of NULL from functions that are declared as returning void.
			if (retmask == kEidosValueMaskVOID)
			{
				return_type_ok = false;
				break;
			}
			
			return;
		}
		case EidosValueType::kValueLogical:	return_type_ok = !!(retmask & kEidosValueMaskLogical);	break;
		case EidosValueType::kValueInt:		return_type_ok = !!(retmask & kEidosValueMaskInt);		break;
		case EidosValueType::kValueFloat:	return_type_ok = !!(retmask & kEidosValueMaskFloat);	break;
		case EidosValueType::kValueString:	return_type_ok = !!(retmask & kEidosValueMaskString);	break;
		case EidosValueType::kValueObject:
			return_type_ok = !!(retmask & kEidosValueMaskObject);
			
			// If the return is object type, and is allowed to be object type, and an object element type was specified
			// in the signature, check the object element type of the return.  Note this uses pointer equality!
			if (return_type_ok && return_class_ && (((EidosValue_Object &)p_result).Class() != return_class_))
			{
				if (!((EidosValue_Object &)p_result).Class()->IsSubclassOfClass(return_class_))
					EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckAggregateReturn): object return value cannot be element type " << p_result.ElementType() << " for " << CallType() << " " << call_name_ << "(); expected object element type " << return_class_->ClassNameForDisplay() << "." << EidosTerminate(nullptr);
			}
			break;
	}
	
	if (!return_type_ok)
	{
		// We try to emit more helpful error messages when void is involved in the type mismatch
		if (retmask == kEidosValueMaskVOID)
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckAggregateReturn): return value must be void for " << CallType() << " " << call_name_ << "(); use a \"return;\" statement if you wish to explicitly return with no return value." << EidosTerminate(nullptr);
		else if (p_result.Type() == EidosValueType::kValueVOID)
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckAggregateReturn): return value cannot be void for " << CallType() << " " << call_name_ << "(); use a \"return\" statement to explicitly return a value." << EidosTerminate(nullptr);
		else
			EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckAggregateReturn): return value cannot be type " << p_result.Type() << " for " << CallType() << " " << call_name_ << "()." << EidosTerminate(nullptr);
	}
	
	bool return_is_singleton = !!(retmask & kEidosValueMaskSingleton);
	
	if (return_is_singleton && ((size_t)p_result.Count() > p_expected_size))
		EIDOS_TERMINATION << "ERROR (EidosCallSignature::CheckAggregateReturn): return value must be a singleton (size() == 1) for " << CallType() << " " << call_name_ << "." << EidosTerminate(nullptr);
}

std::string EidosCallSignature::CallDelegate(void) const
{
	return "";
}

std::string EidosCallSignature::SignatureString(void) const
{
	std::ostringstream ss;
	
	ss << *this;
	
	return ss.str();
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosCallSignature &p_signature)
{
	//
	//	Note this logic is paralleled in +[NSAttributedString eidosAttributedStringForCallSignature:].
	//	These two should be kept in synch so the user-visible format of signatures is consistent.
	//
	
	p_outstream << p_signature.CallPrefix();	// "", "– ", or "+ " depending on our subclass
	
	p_outstream << "(" << StringForEidosValueMask(p_signature.return_mask_, p_signature.return_class_, "", nullptr);
	
	p_outstream << ")" << p_signature.call_name_ << "(";
	
	int arg_mask_count = (int)p_signature.arg_masks_.size();
	
	if (arg_mask_count == 0)
	{
		p_outstream << gEidosStr_void;
	}
	else
	{
		for (int arg_index = 0; arg_index < arg_mask_count; ++arg_index)
		{
			EidosValueMask type_mask = p_signature.arg_masks_[arg_index];
			const std::string &arg_name = p_signature.arg_names_[arg_index];
			const EidosClass *arg_obj_class = p_signature.arg_classes_[arg_index];
			EidosValue_SP arg_default = p_signature.arg_defaults_[arg_index];
			
			// skip private arguments
			if ((arg_name.length() >= 1) && (arg_name[0] == '_'))
				continue;
			
			if (arg_index > 0)
				p_outstream << ", ";
			
			p_outstream << StringForEidosValueMask(type_mask, arg_obj_class, arg_name, arg_default.get());
		}
	}
	
	p_outstream << ")";
	
	// if the function is provided by a delegate, show the delegate's name
	p_outstream << p_signature.CallDelegate();
	
	return p_outstream;
}

bool CompareEidosCallSignatures(const EidosCallSignature_CSP &p_i, const EidosCallSignature_CSP &p_j)
{
	return (p_i->call_name_ < p_j->call_name_);
}


//
//	EidosFunctionSignature
//
#pragma mark -
#pragma mark EidosFunctionSignature
#pragma mark -

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask)
	: EidosCallSignature(p_function_name, p_return_mask), internal_function_(p_function_ptr)
{
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask, const EidosClass *p_return_class)
	: EidosCallSignature(p_function_name, p_return_mask, p_return_class), internal_function_(p_function_ptr)
{
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask, std::string p_delegate_name)
	: EidosCallSignature(p_function_name, p_return_mask), internal_function_(p_function_ptr), delegate_name_(std::move(p_delegate_name))
{
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask, const EidosClass *p_return_class, std::string p_delegate_name)
	: EidosCallSignature(p_function_name, p_return_mask, p_return_class), internal_function_(p_function_ptr), delegate_name_(std::move(p_delegate_name))
{
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, const std::string &p_script_string, EidosValueMask p_return_mask)
	: EidosCallSignature(p_function_name, p_return_mask)
{
	ProcessEidosScript(p_script_string);
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, const std::string &p_script_string, EidosValueMask p_return_mask, const EidosClass *p_return_class)
	: EidosCallSignature(p_function_name, p_return_mask, p_return_class)
{
	ProcessEidosScript(p_script_string);
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, const std::string &p_script_string, EidosValueMask p_return_mask, std::string p_delegate_name)
	: EidosCallSignature(p_function_name, p_return_mask), delegate_name_(std::move(p_delegate_name))
{
	ProcessEidosScript(p_script_string);
}

EidosFunctionSignature::EidosFunctionSignature(const std::string &p_function_name, const std::string &p_script_string, EidosValueMask p_return_mask, const EidosClass *p_return_class, std::string p_delegate_name)
	: EidosCallSignature(p_function_name, p_return_mask, p_return_class), delegate_name_(std::move(p_delegate_name))
{
	ProcessEidosScript(p_script_string);
}

void EidosFunctionSignature::ProcessEidosScript(const std::string &p_script_string)
{
	// This method is for built-in functions implemented in Eidos; they have no position in the user's script string
	EidosScript *source_script = new EidosScript(p_script_string);
	
	source_script->Tokenize();
	source_script->ParseInterpreterBlockToAST(false);
	
	body_script_ = source_script;
}

EidosFunctionSignature::~EidosFunctionSignature(void)
{
	if (body_script_)
		delete body_script_;
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
	if (delegate_name_.size())
	{
		std::string delegate_tag;
		
		delegate_tag += " <";
		delegate_tag += delegate_name_;
		delegate_tag += ">";
		
		return delegate_tag;
	}
	
	return "";
}

bool CompareEidosFunctionSignatures(const EidosFunctionSignature_CSP &p_i, const EidosFunctionSignature_CSP &p_j)
{
	return (p_i->call_name_ < p_j->call_name_);
}


//
//	EidosMethodSignature
//
#pragma mark -
#pragma mark EidosMethodSignature
#pragma mark -

EidosMethodSignature::EidosMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, bool p_is_class_method)
	: EidosCallSignature(p_function_name, p_return_mask), is_class_method(p_is_class_method)
{
}

EidosMethodSignature::EidosMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosClass *p_return_class, bool p_is_class_method)
	: EidosCallSignature(p_function_name, p_return_mask, p_return_class), is_class_method(p_is_class_method)
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
#pragma mark -

EidosInstanceMethodSignature::EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask)
	: EidosMethodSignature(p_function_name, p_return_mask, false), accelerated_imp_(false)
{
}

EidosInstanceMethodSignature::EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosClass *p_return_class)
	: EidosMethodSignature(p_function_name, p_return_mask, p_return_class, false), accelerated_imp_(false)
{
}

std::string EidosInstanceMethodSignature::CallPrefix(void) const
{
	return "–\u00A0";	// en-dash, non-breaking space
}

EidosInstanceMethodSignature::~EidosInstanceMethodSignature(void)
{
}

EidosInstanceMethodSignature *EidosInstanceMethodSignature::DeclareAcceleratedImp(Eidos_AcceleratedMethodImp p_imper)
{
	/*
	 BCH 1/2/2023: Commented out these checks.  They are well-intentioned but excessively strict.  The class itself knows
	 what it is doing.  Some methods might consistently return all one type or all a different type, depending on the
	 parameters passed; there is nothing wrong with that.  It would be more unusual, but the same is technically true
	 for the object element class, too.  And in the end, the implementation could concatenate values of different types
	 together on its own, if that turned out to be necessary.  I'm removing this because it is now in the way.
	 
	uint32_t retmask = (return_mask_ & kEidosValueMaskFlagStrip);
	
	if ((retmask != kEidosValueMaskVOID) && 
		(retmask != kEidosValueMaskNULL) && 
		(retmask != kEidosValueMaskLogical) && 
		(retmask != kEidosValueMaskInt) && 
		(retmask != kEidosValueMaskFloat) && 
		(retmask != kEidosValueMaskString) && 
		(retmask != kEidosValueMaskObject))
		EIDOS_TERMINATION << "ERROR (EidosInstanceMethodSignature::DeclareAcceleratedImp): (internal error) only methods returning one guaranteed type may be accelerated." << EidosTerminate(nullptr);
	
	if ((retmask == (kEidosValueMaskObject | kEidosValueMaskSingleton)) && (return_class_ == nullptr))
		EIDOS_TERMINATION << "ERROR (EidosInstanceMethodSignature::DeclareAcceleratedImp): (internal error) only object methods that declare their class may be accelerated." << EidosTerminate(nullptr);
	*/
	
	accelerated_imp_ = true;
	accelerated_imper_ = p_imper;
	
	return this;
}


//
//	EidosClassMethodSignature
//
#pragma mark -
#pragma mark EidosClassMethodSignature
#pragma mark -

EidosClassMethodSignature::EidosClassMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask)
	: EidosMethodSignature(p_function_name, p_return_mask, true)
{
}

EidosClassMethodSignature::EidosClassMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosClass *p_return_class)
	: EidosMethodSignature(p_function_name, p_return_mask, p_return_class, true)
{
}

std::string EidosClassMethodSignature::CallPrefix(void) const
{
	return "+\u00A0";	// non-breaking space
}

EidosClassMethodSignature::~EidosClassMethodSignature(void)
{
}


































































