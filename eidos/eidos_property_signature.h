//
//  eidos_property_signature.h
//  Eidos
//
//  Created by Ben Haller on 8/3/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
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


#ifndef __Eidos__eidos_property_signature__
#define __Eidos__eidos_property_signature__

#include "eidos_value.h"


// This typedef is for an "accelerated property getter".  These are static member functions on a class, designed to provide a whole
// vector of property values given a buffer of EidosObjectElements.  The getter is expected to return the correct type for the
// property (this is checked).  The getter is guaranteed that the EidosObjectElements are of the correct class; it is allowed to
// do a cast of p_values directly to its own type without checking, according to the calling conventions used here.
typedef EidosValue *(*Eidos_AcceleratedPropertyGetter)(EidosObjectElement **p_values, size_t p_values_size);

// This typedef is for an "accelerated property setter".  These are static member functions on a class, designed to set a property
// value across a buffer of EidosObjectElements.  This is more complex than the getter case, because there are two possibilities:
// p_source could be a singleton, providing one value to be set across the whole buffer, OR it could be a vector of length equal
// to the buffer size.  It is guaranteed to be one of those two things; the setter does not need to cover the case where the length
// of p_source is not singleton but not equal to p_values_size.  As with accelerated getters, p_values is guaranteed by the caller
// to be of the correct class, and may be cast directly.  (This is actually guaranteed and checked by the property signature, so if
// the signature is declared incorrectly then a mismatch is possible; but that is not the getter/setter's problem to detect.)  The
// type of p_source is also checked against the signature, and so may be assumed to be of the declared type.
typedef void (*Eidos_AcceleratedPropertySetter)(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);


class EidosPropertySignature
{
public:
	std::string property_name_;
	EidosGlobalStringID property_id_;
	
	bool read_only_;									// true if the property is read-only, false if it is read-write
	EidosValueMask value_mask_;							// a mask for the type returned; singleton is used, optional is not
	const EidosObjectClass *value_class_;				// optional type-check for object values; used only if this is not nullptr
	
	bool accelerated_get_;									// if true, can be read using a fast-access GetProperty_Accelerated_X() method
	Eidos_AcceleratedPropertyGetter accelerated_getter;		// a pointer to a (static member) function that handles the accelerated get
	
	bool accelerated_set_;									// if true, can be written using a fast-access SetProperty_Accelerated_X() method
	Eidos_AcceleratedPropertySetter accelerated_setter;		// a pointer to a (static member) function that handles the accelerated set
	
	EidosPropertySignature(const EidosPropertySignature&) = delete;					// no copying
	EidosPropertySignature& operator=(const EidosPropertySignature&) = delete;		// no copying
	EidosPropertySignature(void) = delete;											// no null construction
	virtual ~EidosPropertySignature(void);
	
	EidosPropertySignature(const std::string &p_property_name, bool p_read_only, EidosValueMask p_value_mask);
	EidosPropertySignature(const std::string &p_property_name, bool p_read_only, EidosValueMask p_value_mask, const EidosObjectClass *p_value_class);
	
	// check arguments and returns
	bool CheckAssignedValue(const EidosValue &p_value) const;	// checks a vector being assigned into a whole object; true is exact match, false is implicit type conversion
	void CheckResultValue(const EidosValue &p_value) const;	// checks the result from a single element
	void CheckAggregateResultValue(const EidosValue &p_value, size_t p_expected_size) const;	// checks the result from a vector
	
	// informational strings about the property
	std::string PropertyType(void) const;				// "read-only" or "read-write"
	std::string PropertySymbol(void) const;				// "=>" or "–>"
	
	// property access acceleration
	EidosPropertySignature *DeclareAcceleratedGet(Eidos_AcceleratedPropertyGetter p_getter);
	EidosPropertySignature *DeclareAcceleratedSet(Eidos_AcceleratedPropertySetter p_setter);
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosPropertySignature &p_signature);
bool CompareEidosPropertySignatures(const EidosPropertySignature *p_i, const EidosPropertySignature *p_j);


#endif /* defined(__Eidos__eidos_property_signature__) */



















































