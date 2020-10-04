//
//  eidos_test_element.cpp
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
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


#include "eidos_test_element.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_globals.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <algorithm>
#include <string>
#include <vector>


// See EidosTestElement::GetProperty(), EidosTestElement::ExecuteInstanceMethod, and 
static std::vector<EidosTestElement *> inc_element_thunk;
static std::vector<EidosTestElement *> sq_element_thunk;

void EidosTestElement::FreeThunks(void)
{
	// Valgrind doesn't seem happy with our spuriously allocated test elements just being referenced
	// by a vector; maybe std::vector does not guarantee alignment or something.  Anyway, if we free
	// the elements then Valgrind knows for sure they're not leaked.
	for (auto thunk_iter : inc_element_thunk)
		delete (thunk_iter);
	
	inc_element_thunk.clear();
	
	for (auto thunk_iter : sq_element_thunk)
		delete (thunk_iter);
	
	sq_element_thunk.clear();
}


//
//	EidosTestElement
//
#pragma mark -
#pragma mark EidosTestElement
#pragma mark -

EidosTestElement::EidosTestElement(int64_t p_value) : yolk_(p_value)
{
}

const EidosObjectClass *EidosTestElement::Class(void) const
{
	return gEidosTestElement_Class;
}

EidosValue_SP EidosTestElement::GetProperty(EidosGlobalStringID p_property_id)
{
	if (p_property_id == gEidosID__yolk)				// ACCELERATED
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(yolk_));
	else if (p_property_id == gEidosID__increment)
	{
		// The way we handle the increment property is extremely questionable; we create a new
		// EidosTestElement object that is not owned by anyone, so it ends up as a leak in
		// Instruments.  This doesn't matter, since EidosTestElement is only used in test code,
		// but it clutters up leak reports confusingly.  To get rid of those leak reports, we
		// keep a static vector of pointers to the leaked elements, so they are no longer
		// considered leaks.  This is an ugly hack, but is completely harmless.
		EidosTestElement *inc_element = new EidosTestElement(yolk_ + 1);
		
		inc_element_thunk.push_back(inc_element);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(inc_element, gEidosTestElement_Class));
	}
	
	// all others, including gID_none
	else
		return EidosObjectElement::GetProperty(p_property_id);
}

EidosValue *EidosTestElement::GetProperty_Accelerated__yolk(EidosObjectElement **p_elements, size_t p_elements_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_elements_size);
	
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		EidosTestElement *element = (EidosTestElement *)(p_elements[element_index]);
		
		int_result->set_int_no_check(element->yolk_, element_index);
	}
	
	return int_result;
}

void EidosTestElement::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	if (p_property_id == gEidosID__yolk)				// ACCELERATED
	{
		yolk_ = p_value.IntAtIndex(0, nullptr);
		return;
	}
	
	// all others, including gID_none
	else
		return EidosObjectElement::SetProperty(p_property_id, p_value);
}

void EidosTestElement::SetProperty_Accelerated__yolk(EidosObjectElement **p_elements, size_t p_elements_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
			((EidosTestElement *)(p_elements[element_index]))->yolk_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
			((EidosTestElement *)(p_elements[element_index]))->yolk_ = source_data[element_index];
	}
}

EidosValue_SP EidosTestElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		//case gEidosID__cubicYolk:	return ExecuteMethod_Accelerated_cubicYolk(p_method_id, p_arguments, p_interpreter);
		case gEidosID__squareTest:	return ExecuteMethod_squareTest(p_method_id, p_arguments, p_interpreter);
		default:					return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

EidosValue_SP EidosTestElement::ExecuteMethod_Accelerated_cubicYolk(EidosObjectElement **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_elements_size);
	
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		EidosTestElement *element = (EidosTestElement *)(p_elements[element_index]);
		
		int_result->set_int_no_check(element->yolk_ * element->yolk_ * element->yolk_, element_index);
	}
	
	return EidosValue_SP(int_result);
}

EidosValue_SP EidosTestElement::ExecuteMethod_squareTest(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The way we handle the squareTest property is extremely questionable; we create a new
	// EidosTestElement object that is not owned by anyone, so it ends up as a leak in
	// Instruments.  This doesn't matter, since EidosTestElement is only used in test code,
	// but it clutters up leak reports confusingly.  To get rid of those leak reports, we
	// keep a static vector of pointers to the leaked elements, so they are no longer
	// considered leaks.  This is an ugly hack, but is completely harmless.
	EidosTestElement *sq_element = new EidosTestElement(yolk_ * yolk_);
	
	sq_element_thunk.push_back(sq_element);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(sq_element, gEidosTestElement_Class));
}


//
//	EidosTestElement_Class
//
#pragma mark -
#pragma mark EidosTestElement_Class
#pragma mark -

class EidosTestElement_Class : public EidosObjectClass_Retained
{
public:
	EidosTestElement_Class(const EidosTestElement_Class &p_original) = delete;	// no copy-construct
	EidosTestElement_Class& operator=(const EidosTestElement_Class&) = delete;	// no copying
	inline EidosTestElement_Class(void) { }
	
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};

EidosObjectClass *gEidosTestElement_Class = new EidosTestElement_Class();


const std::string &EidosTestElement_Class::ElementType(void) const
{
	return gEidosStr__TestElement;
}

const std::vector<EidosPropertySignature_CSP> *EidosTestElement_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*EidosObjectClass::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr__yolk,		false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(EidosTestElement::GetProperty_Accelerated__yolk)->DeclareAcceleratedSet(EidosTestElement::SetProperty_Accelerated__yolk));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr__increment,	true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElement_Class)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosTestElement_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*EidosObjectClass::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__cubicYolk, kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedImp(EidosTestElement::ExecuteMethod_Accelerated_cubicYolk));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__squareTest, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElement_Class)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}


































































