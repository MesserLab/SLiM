//
//  eidos_property_signature.cpp
//  Eidos
//
//  Created by Ben Haller on 8/3/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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


#include "eidos_property_signature.h"
#include "eidos_value.h"

#include <string>


EidosPropertySignature::EidosPropertySignature(const std::string &p_property_name, bool p_read_only, EidosValueMask p_value_mask)
	: property_name_(p_property_name), property_id_(Eidos_GlobalStringIDForString(p_property_name)), read_only_(p_read_only), value_mask_(p_value_mask), value_class_(nullptr), accelerated_get_(false), accelerated_set_(false)
{
	if (!read_only_ && !(value_mask_ & kEidosValueMaskSingleton))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) read-write property " << property_name_ << " must produce a singleton value according to Eidos semantics." << EidosTerminate(nullptr);
	if (value_mask_ & kEidosValueMaskVOID)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) properties are not allowed to return void." << EidosTerminate(nullptr);
	if (value_mask_ & kEidosValueMaskNULL)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) properties are not allowed to return NULL." << EidosTerminate(nullptr);
}

EidosPropertySignature::EidosPropertySignature(const std::string &p_property_name, bool p_read_only, EidosValueMask p_value_mask, const EidosObjectClass *p_value_class)
	: property_name_(p_property_name), property_id_(Eidos_GlobalStringIDForString(p_property_name)), read_only_(p_read_only), value_mask_(p_value_mask), value_class_(p_value_class), accelerated_get_(false), accelerated_set_(false)
{
	if (!read_only_ && !(value_mask_ & kEidosValueMaskSingleton))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) read-write property " << property_name_ << " must produce a singleton value according to Eidos semantics." << EidosTerminate(nullptr);
	if (value_mask_ & kEidosValueMaskVOID)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) properties are not allowed to return void." << EidosTerminate(nullptr);
	if (value_mask_ & kEidosValueMaskNULL)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) properties are not allowed to return NULL." << EidosTerminate(nullptr);
}

EidosPropertySignature::~EidosPropertySignature(void)
{
}

bool EidosPropertySignature::CheckAssignedValue(const EidosValue &p_value) const
{
	uint32_t retmask = value_mask_;
	bool value_type_ok = true;
	bool value_exact_match = true;
	
	switch (p_value.Type())
	{
		case EidosValueType::kValueVOID:
			value_type_ok = false;	// never OK regardless of retmask
			break;
		case EidosValueType::kValueNULL:
			// BCH 30 January 2017: setting NULL into a property used to be allowed here without declaration (as it is
			// when getting the value of a property), but I think that was just a bug.  I'm modifying this to throw an
			// exception unless NULL is explicitly declared as acceptable in the signature.
			
			// BCH 11 December 2017: note that NULL can no longer be declared in a property signature, so setting
			// a property to NULL will always raise now; this is official Eidos semantics now, strict no-no, so
			// rather than checking retmask we just set value_type_ok to false unconditionally now.
			//value_type_ok = !!(retmask & kEidosValueMaskNULL);
			value_type_ok = false;
			break;
		case EidosValueType::kValueLogical:
			value_type_ok = !!(retmask & (kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskFloat));		// can give logical to an int or float property
			value_exact_match = !!(retmask & kEidosValueMaskLogical);
			break;
		case EidosValueType::kValueInt:
			value_type_ok = !!(retmask & (kEidosValueMaskInt | kEidosValueMaskFloat));								// can give int to a float property
			value_exact_match = !!(retmask & kEidosValueMaskInt);
			break;
		case EidosValueType::kValueFloat:
			value_type_ok = !!(retmask & kEidosValueMaskFloat);
			break;
		case EidosValueType::kValueString:
			value_type_ok = !!(retmask & kEidosValueMaskString);
			break;
		case EidosValueType::kValueObject:
			value_type_ok = !!(retmask & kEidosValueMaskObject);
			
			// If the value is object type, and is allowed to be object type, and an object element type was specified
			// in the signature, check the object element type of the value.  Note this uses pointer equality!
			// This check is applied only if the value contains elements, since an empty object does not know its type.
			if (value_type_ok && value_class_ && (((EidosValue_Object *)&p_value)->Class() != value_class_) && (p_value.Count() > 0))
			{
				EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckAssignedValue): object value cannot be object element type " << p_value.ElementType() << " for " << PropertyType() << " property " << property_name_ << "; expected object element type " << value_class_->ElementType() << "." << EidosTerminate(nullptr);
			}
			break;
	}
	
	if (!value_type_ok)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckAssignedValue): value cannot be type " << p_value.Type() << " for " << PropertyType() << " property " << property_name_ << "." << EidosTerminate(nullptr);
	
	// No check for size, because we're checking a whole vector being assigned into an object; EidosValue_Object will check the sizes
	
	return value_exact_match;
}

void EidosPropertySignature::CheckResultValue(const EidosValue &p_value) const
{
	uint32_t retmask = value_mask_;
	bool value_type_ok = true;
	
	switch (p_value.Type())
	{
		case EidosValueType::kValueVOID:
			// void is not allowed as a property value, getting or setting, ever.
			EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckResultValue): (internal error) void returned for property " << property_name_ << "." << EidosTerminate(nullptr);
			return;
		case EidosValueType::kValueNULL:
			// A return type of NULL is always allowed, in fact; we don't want to have to specify this in the return type
			// This is a little fishy, but since NULL is used to indicate error conditions, NULL returns are exceptional,
			// and the return type indicates the type ordinarily returned in non-exceptional cases.  We just return here,
			// since we also don't want to do the singleton check below (since it would raise too).
			
			// BCH 30 January 2017: Modifying this a bit.  The policy is still that a return type of NULL is always
			// allowed.  However, this is incompatible with accelerated gets, so if a property is declared as accelerated,
			// the code here will throw an error.  This should probably never happen, since if someone tries to accelerate
			// a property that can return NULL they will immediately realize the error of their ways, as they will find it
			// to be impossible to implement.  :->  Still, for clarity and possible debugging value, I'm adding a check.
			
			// BCH 11 December 2017: note that NULL can no longer be declared in a property signature, and is no longer
			// ever allowed as a value for a property, so the above comments are obsolete.  This is now official Eidos
			// semantics, to allow guaranteed one-to-one matching of objects and their singleton properties: NULL is not
			// allowed as a property value, getting or setting, ever.
			EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckResultValue): (internal error) NULL returned for property " << property_name_ << "." << EidosTerminate(nullptr);
			return;
		case EidosValueType::kValueLogical:	value_type_ok = !!(retmask & kEidosValueMaskLogical);	break;
		case EidosValueType::kValueInt:		value_type_ok = !!(retmask & kEidosValueMaskInt);		break;
		case EidosValueType::kValueFloat:	value_type_ok = !!(retmask & kEidosValueMaskFloat);		break;
		case EidosValueType::kValueString:	value_type_ok = !!(retmask & kEidosValueMaskString);		break;
		case EidosValueType::kValueObject:
			value_type_ok = !!(retmask & kEidosValueMaskObject);
			
			// If the value is object type, and is allowed to be object type, and an object element type was specified
			// in the signature, check the object element type of the value.  Note this uses pointer equality!
			// This check is applied only if the value contains elements, since an empty object does not know its type.
			if (value_type_ok && value_class_ && (((EidosValue_Object *)&p_value)->Class() != value_class_) && (p_value.Count() > 0))
			{
				EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckResultValue): (internal error) object value cannot be object element type " << p_value.ElementType() << " for " << PropertyType() << " property " << property_name_ << "; expected object element type " << value_class_->ElementType() << "." << EidosTerminate(nullptr);
			}
			break;
	}
	
	if (!value_type_ok)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckResultValue): (internal error) value cannot be type " << p_value.Type() << " for " << PropertyType() << " property " << property_name_ << "." << EidosTerminate(nullptr);
	
	bool return_is_singleton = !!(retmask & kEidosValueMaskSingleton);
	
	if (return_is_singleton && (p_value.Count() != 1))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckResultValue): (internal error) value must be a singleton (size() == 1) for " << PropertyType() << " property " << property_name_ << ", but size() == " << p_value.Count() << "." << EidosTerminate(nullptr);
}

void EidosPropertySignature::CheckAggregateResultValue(const EidosValue &p_value, size_t p_expected_size) const
{
	uint32_t retmask = value_mask_;
	bool value_type_ok = true;
	
	switch (p_value.Type())
	{
		case EidosValueType::kValueVOID:
			// void is not allowed as a property value, getting or setting, ever.
			EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckAggregateResultValue): (internal error) void returned for property " << property_name_ << "." << EidosTerminate(nullptr);
			return;
		case EidosValueType::kValueNULL:
			// BCH 11 December 2017: NULL is not allowed as a property value, getting or setting, ever.
			EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckAggregateResultValue): (internal error) NULL returned for property " << property_name_ << "." << EidosTerminate(nullptr);
			return;
		case EidosValueType::kValueLogical:	value_type_ok = !!(retmask & kEidosValueMaskLogical);	break;
		case EidosValueType::kValueInt:		value_type_ok = !!(retmask & kEidosValueMaskInt);		break;
		case EidosValueType::kValueFloat:	value_type_ok = !!(retmask & kEidosValueMaskFloat);		break;
		case EidosValueType::kValueString:	value_type_ok = !!(retmask & kEidosValueMaskString);		break;
		case EidosValueType::kValueObject:
			value_type_ok = !!(retmask & kEidosValueMaskObject);
			
			// If the value is object type, and is allowed to be object type, and an object element type was specified
			// in the signature, check the object element type of the value.  Note this uses pointer equality!
			// This check is applied only if the value contains elements, since an empty object does not know its type.
			if (value_type_ok && value_class_ && (((EidosValue_Object *)&p_value)->Class() != value_class_) && (p_value.Count() > 0))
			{
				EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckAggregateResultValue): (internal error) object value cannot be object element type " << p_value.ElementType() << " for " << PropertyType() << " property " << property_name_ << "; expected object element type " << value_class_->ElementType() << "." << EidosTerminate(nullptr);
			}
			break;
	}
	
	if (!value_type_ok)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckAggregateResultValue): (internal error) value cannot be type " << p_value.Type() << " for " << PropertyType() << " property " << property_name_ << "." << EidosTerminate(nullptr);
	
	bool return_is_singleton = !!(retmask & kEidosValueMaskSingleton);
	
	if (return_is_singleton && ((size_t)p_value.Count() != p_expected_size))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckAggregateResultValue): (internal error) value must be a singleton (size() == 1) for " << PropertyType() << " property " << property_name_ << "." << EidosTerminate(nullptr);
}

std::string EidosPropertySignature::PropertyType(void) const
{
	return (read_only_ ? "read-only" : "read-write");
}

std::string EidosPropertySignature::PropertySymbol(void) const
{
	return (read_only_ ? "=>" : "<–>");
}

EidosPropertySignature *EidosPropertySignature::DeclareAcceleratedGet(Eidos_AcceleratedPropertyGetter p_getter)
{
	uint32_t retmask = (value_mask_ & kEidosValueMaskFlagStrip);
	
	if ((retmask != kEidosValueMaskLogical) && 
		(retmask != kEidosValueMaskInt) && 
		(retmask != kEidosValueMaskFloat) && 
		(retmask != kEidosValueMaskString) && 
		(retmask != kEidosValueMaskObject))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedGet): (internal error) only properties returning one guaranteed type may be accelerated." << EidosTerminate(nullptr);
	
	if ((retmask == (kEidosValueMaskObject | kEidosValueMaskSingleton)) && (value_class_ == nullptr))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedGet): (internal error) only object properties that declare their class may be accelerated." << EidosTerminate(nullptr);
	
	accelerated_get_ = true;
	accelerated_getter = p_getter;
	
	return this;
}

EidosPropertySignature *EidosPropertySignature::DeclareAcceleratedSet(Eidos_AcceleratedPropertySetter p_setter)
{
	if (read_only_)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedSet): (internal error) only read-write properties may be accelerated." << EidosTerminate(nullptr);
	
	uint32_t retmask = value_mask_;
	bool return_is_singleton = !!(retmask & kEidosValueMaskSingleton);
	
	if (!return_is_singleton)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedSet): (internal error) only singleton properties may be accelerated." << EidosTerminate(nullptr);
	
	if ((retmask != (kEidosValueMaskLogical | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskInt | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskFloat | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskString | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskObject | kEidosValueMaskSingleton)))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedSet): (internal error) only properties returning one guaranteed type may be accelerated." << EidosTerminate(nullptr);
	
	if ((retmask == (kEidosValueMaskObject | kEidosValueMaskSingleton)) && (value_class_ == nullptr))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedSet): (internal error) only object properties that declare their class may be accelerated." << EidosTerminate(nullptr);
	
	accelerated_set_ = true;
	accelerated_setter = p_setter;
	
	return this;
}

// This is unused except by debugging code and in the debugger itself
std::ostream &operator<<(std::ostream &p_outstream, const EidosPropertySignature &p_signature)
{
	p_outstream << p_signature.property_name_ << " " << p_signature.PropertySymbol() << " (";
	p_outstream << StringForEidosValueMask(p_signature.value_mask_, p_signature.value_class_, "", nullptr) << ")";
	
	return p_outstream;
}

bool CompareEidosPropertySignatures(const EidosPropertySignature_CSP &p_i, const EidosPropertySignature_CSP &p_j)
{
	return (p_i->property_name_ < p_j->property_name_);
}



























































