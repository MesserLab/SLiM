//
//  eidos_test_element.h
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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

/*
 
 The class Eidos_TestElement is an object element class (i.e. an element class for EidosValue_Object) for testing of Eidos's objects.
 It just encapsulates an integer value, so it is not useful for anything but testing.
 
 */

#ifndef __Eidos__eidos_path_element__
#define __Eidos__eidos_path_element__

#include "eidos_value.h"


extern EidosObjectClass *gEidos_TestElementClass;


class Eidos_TestElement : public EidosObjectElementInternal
{
private:
	int64_t yolk_;
	
public:
	Eidos_TestElement(const Eidos_TestElement &p_original) = delete;	// no copy-construct
	Eidos_TestElement& operator=(const Eidos_TestElement&) = delete;	// no copying
	
	explicit Eidos_TestElement(int64_t p_value);
	
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


#endif /* defined(__Eidos__eidos_path_element__) */





































































