//
//  eidos_class_TestElement.h
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
//  Copyright (c) 2015-2021 Philipp Messer.  All rights reserved.
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

#ifndef __Eidos__eidos_class_test_element__
#define __Eidos__eidos_class_test_element__


#include "eidos_value.h"


extern EidosClass *gEidosTestElement_Class;


class EidosTestElement : public EidosDictionaryRetained
{
private:
	typedef EidosDictionaryRetained super;

private:
	int64_t yolk_;
	
public:
	EidosTestElement(const EidosTestElement &p_original) = delete;	// no copy-construct
	EidosTestElement& operator=(const EidosTestElement&) = delete;	// no copying
	
	explicit EidosTestElement(int64_t p_value);
	virtual ~EidosTestElement(void) override;
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	static EidosValue_SP ExecuteMethod_Accelerated_cubicYolk(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_squareTest(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated__yolk(EidosObject **p_elements, size_t p_elements_size);
	static void SetProperty_Accelerated__yolk(EidosObject **p_elements, size_t p_elements_size, const EidosValue &p_source, size_t p_source_size);
};

class EidosTestElement_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	EidosTestElement_Class(const EidosTestElement_Class &p_original) = delete;	// no copy-construct
	EidosTestElement_Class& operator=(const EidosTestElement_Class&) = delete;	// no copying
	inline EidosTestElement_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
};


#endif /* defined(__Eidos__eidos_class_test_element__) */





































































