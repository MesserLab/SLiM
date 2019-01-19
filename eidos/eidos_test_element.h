//
//  eidos_test_element.h
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
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

/*
 
 The class EidosTestElement is an object element class (i.e. an element class for EidosValue_Object) for testing of Eidos's objects.
 It just encapsulates an integer value, so it is not useful for anything but testing.
 
 */

#ifndef __Eidos__eidos_path_element__
#define __Eidos__eidos_path_element__

#include "eidos_value.h"


extern EidosObjectClass *gEidosTestElement_Class;


class EidosTestElement : public EidosObjectElementInternal
{
private:
	int64_t yolk_;
	
public:
	EidosTestElement(const EidosTestElement &p_original) = delete;	// no copy-construct
	EidosTestElement& operator=(const EidosTestElement&) = delete;	// no copying
	
	explicit EidosTestElement(int64_t p_value);
	
	static void FreeThunks(void);										// a hack to de-confuse Valgrind
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_cubicYolk(EidosObjectElement **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_squareTest(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated__yolk(EidosObjectElement **p_elements, size_t p_elements_size);
	static void SetProperty_Accelerated__yolk(EidosObjectElement **p_elements, size_t p_elements_size, const EidosValue &p_source, size_t p_source_size);
};


#endif /* defined(__Eidos__eidos_path_element__) */





































































