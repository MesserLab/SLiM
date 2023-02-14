//
//  eidos_class_TestElement.h
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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
 
 Two classes are defined in this file, EidosTestElement and EidosTestElementNRR.  They are object element classes
 (i.e. element classes for EidosValue_Object) for testing of Eidos's objects.  They are not user-visible.
 
 */

#ifndef __Eidos__eidos_class_test_element__
#define __Eidos__eidos_class_test_element__

#include "eidos_value.h"


//
// EidosTestElement is used for testing.  It is a subclass of EidosDictionaryRetained,
// and is under retain-release.  It is instantiated with a hidden constructor:
//
//		(object<_TestElement>$)_Test(integer$ value)
//

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


//
// EidosTestElementNRR is used for testing.  It is a direct subclass of EidosObject and is
// not under retain-release (thus "NRR").  It is instantiated with a hidden constructor:
//
//		(object<_TestElementNRR>$)_TestNRR(integer$ value)
//

extern EidosClass *gEidosTestElementNRR_Class;


class EidosTestElementNRR : public EidosObject
{
private:
	typedef EidosObject super;

private:
	int64_t yolk_;
	
public:
	EidosTestElementNRR(const EidosTestElementNRR &p_original) = delete;	// no copy-construct
	EidosTestElementNRR& operator=(const EidosTestElementNRR&) = delete;	// no copying
	
	explicit EidosTestElementNRR(int64_t p_value);
	virtual ~EidosTestElementNRR(void) override;
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
};

class EidosTestElementNRR_Class : public EidosClass
{
private:
	typedef EidosClass super;

public:
	EidosTestElementNRR_Class(const EidosTestElementNRR_Class &p_original) = delete;	// no copy-construct
	EidosTestElementNRR_Class& operator=(const EidosTestElementNRR_Class&) = delete;	// no copying
	inline EidosTestElementNRR_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
};




#endif /* defined(__Eidos__eidos_class_test_element__) */





































































