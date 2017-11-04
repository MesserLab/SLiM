//
//  eidos_property_signature.cpp
//  Eidos
//
//  Created by Ben Haller on 8/3/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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

#include <string>


EidosPropertySignature::EidosPropertySignature(const std::string &p_property_name, EidosGlobalStringID p_property_id, bool p_read_only, EidosValueMask p_value_mask)
	: property_name_(p_property_name), property_id_(p_property_id), read_only_(p_read_only), value_mask_(p_value_mask), value_class_(nullptr), accelerated_get_(false), accelerated_set_(false)
{
	if (!read_only_ && !(value_mask_ & kEidosValueMaskSingleton))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) read-write property " << property_name_ << " must produce a singleton value according to Eidos semantics." << EidosTerminate(nullptr);
}

EidosPropertySignature::EidosPropertySignature(const std::string &p_property_name, EidosGlobalStringID p_property_id, bool p_read_only, EidosValueMask p_value_mask, const EidosObjectClass *p_value_class)
	: property_name_(p_property_name), property_id_(p_property_id), read_only_(p_read_only), value_mask_(p_value_mask), value_class_(p_value_class), accelerated_get_(false), accelerated_set_(false)
{
	if (!read_only_ && !(value_mask_ & kEidosValueMaskSingleton))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::EidosPropertySignature): (internal error) read-write property " << property_name_ << " must produce a singleton value according to Eidos semantics." << EidosTerminate(nullptr);
}

EidosPropertySignature::~EidosPropertySignature(void)
{
}

void EidosPropertySignature::CheckAssignedValue(const EidosValue &p_value) const
{
	uint32_t retmask = value_mask_;
	bool value_type_ok = true;
	
	switch (p_value.Type())
	{
		case EidosValueType::kValueNULL:
			// BCH 30 January 2017: setting NULL into a property used to be allowed here without declaration (as it is
			// when getting the value of a property), but I think that was just a bug.  I'm modifying this to throw an
			// exception unless NULL is explicitly declared as acceptable in the signature.
			value_type_ok = !!(retmask & kEidosValueMaskNULL);
			break;
		case EidosValueType::kValueLogical:	value_type_ok = !!(retmask & (kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskFloat)); break;		// can give logical to an int or float property
		case EidosValueType::kValueInt:		value_type_ok = !!(retmask & (kEidosValueMaskInt | kEidosValueMaskFloat)); break;							// can give int to a float property
		case EidosValueType::kValueFloat:	value_type_ok = !!(retmask & kEidosValueMaskFloat); break;
		case EidosValueType::kValueString:	value_type_ok = !!(retmask & kEidosValueMaskString);	 break;
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
}

void EidosPropertySignature::CheckResultValue(const EidosValue &p_value) const
{
	uint32_t retmask = value_mask_;
	bool value_type_ok = true;
	
	switch (p_value.Type())
	{
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
			if (accelerated_get_ && !(retmask & kEidosValueMaskNULL))
				EIDOS_TERMINATION << "ERROR (EidosPropertySignature::CheckResultValue): (internal error) NULL returned for accelerated property " << property_name_ << "." << EidosTerminate(nullptr);
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

std::string EidosPropertySignature::PropertyType(void) const
{
	return (read_only_ ? "read-only" : "read-write");
}

std::string EidosPropertySignature::PropertySymbol(void) const
{
	return (read_only_ ? "=>" : "<–>");
}

EidosPropertySignature *EidosPropertySignature::DeclareAcceleratedGet(void)
{
	uint32_t retmask = value_mask_;
	bool return_is_singleton = !!(retmask & kEidosValueMaskSingleton);
	
	if (!return_is_singleton)
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedGet): (internal error) only singleton properties may be accelerated." << EidosTerminate(nullptr);
	
	if ((retmask != (kEidosValueMaskLogical | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskInt | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskFloat | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskString | kEidosValueMaskSingleton)) && 
		(retmask != (kEidosValueMaskObject | kEidosValueMaskSingleton)))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedGet): (internal error) only properties returning one guaranteed type may be accelerated." << EidosTerminate(nullptr);
	
	if ((retmask == (kEidosValueMaskObject | kEidosValueMaskSingleton)) && (value_class_ == nullptr))
		EIDOS_TERMINATION << "ERROR (EidosPropertySignature::DeclareAcceleratedGet): (internal error) only object properties that declare their class may be accelerated." << EidosTerminate(nullptr);
	
	accelerated_get_ = true;
	
	return this;
}

EidosPropertySignature *EidosPropertySignature::DeclareAcceleratedSet(void)
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
	
	return this;
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosPropertySignature &p_signature)
{
	p_outstream << p_signature.property_name_ << " " << p_signature.PropertySymbol() << " (";
	p_outstream << StringForEidosValueMask(p_signature.value_mask_, p_signature.value_class_, "", nullptr) << ")";
	
	return p_outstream;
}

bool CompareEidosPropertySignatures(const EidosPropertySignature *p_i, const EidosPropertySignature *p_j)
{
	return (p_i->property_name_ < p_j->property_name_);
}



























































