//
//  eidos_property_signature.h
//  Eidos
//
//  Created by Ben Haller on 8/3/15.
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


#ifndef __Eidos__eidos_property_signature__
#define __Eidos__eidos_property_signature__

#include "eidos_value.h"


class EidosPropertySignature
{
public:
	std::string property_name_;
	EidosGlobalStringID property_id_;
	
	bool read_only_;									// true if the property is read-only, false if it is read-write
	EidosValueMask value_mask_;							// a mask for the type returned; singleton is used, optional is not
	const EidosObjectClass *value_class_;				// optional type-check for object values; used only if this is not nullptr
	
	
	EidosPropertySignature(const EidosPropertySignature&) = delete;					// no copying
	EidosPropertySignature& operator=(const EidosPropertySignature&) = delete;		// no copying
	EidosPropertySignature(void) = delete;											// no null construction
	virtual ~EidosPropertySignature(void);
	
	EidosPropertySignature(const std::string &p_property_name, EidosGlobalStringID p_property_id, bool p_read_only, EidosValueMask p_value_mask);
	EidosPropertySignature(const std::string &p_property_name, EidosGlobalStringID p_property_id, bool p_read_only, EidosValueMask p_value_mask, const EidosObjectClass *p_value_class);
	
	// check arguments and returns
	void CheckAssignedValue(const EidosValue &p_value) const;	// checks a vector being assigned into a whole object
	void CheckResultValue(const EidosValue &p_value) const;	// checks the result from a single element
	
	// informational strings about the property
	std::string PropertyType(void) const;				// "read-only" or "read-write"
	std::string PropertySymbol(void) const;				// "=>" or "–>"
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosPropertySignature &p_signature);
bool CompareEidosPropertySignatures(const EidosPropertySignature *p_i, const EidosPropertySignature *p_j);


#endif /* defined(__Eidos__eidos_property_signature__) */



















































