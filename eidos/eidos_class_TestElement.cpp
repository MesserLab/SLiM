//
//  eidos_class_TestElement.cpp
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


#include "eidos_class_TestElement.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_globals.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <algorithm>
#include <string>
#include <vector>


//
//	EidosTestElement
//
#pragma mark -
#pragma mark EidosTestElement
#pragma mark -

EidosTestElement::EidosTestElement(int64_t p_value) : yolk_(p_value)
{
}

EidosTestElement::~EidosTestElement(void)
{
	//std::cout << "~EidosTestElement : " << yolk_ << std::endl;
}

const EidosClass *EidosTestElement::Class(void) const
{
	return gEidosTestElement_Class;
}

void EidosTestElement::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP EidosTestElement::GetProperty(EidosGlobalStringID p_property_id)
{
	if (p_property_id == gEidosID__yolk)				// ACCELERATED
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(yolk_));
	else if (p_property_id == gEidosID__increment)
	{
		EidosTestElement *inc_element = new EidosTestElement(yolk_ + 1);
		EidosValue_SP retval(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(inc_element, gEidosTestElement_Class));
		inc_element->Release();	// retval now owns it; release the retain from new
		
		return retval;
	}
	
	// all others, including gID_none
	else
		return super::GetProperty(p_property_id);
}

EidosValue *EidosTestElement::GetProperty_Accelerated__yolk(EidosObject **p_elements, size_t p_elements_size)
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
		return super::SetProperty(p_property_id, p_value);
}

void EidosTestElement::SetProperty_Accelerated__yolk(EidosObject **p_elements, size_t p_elements_size, const EidosValue &p_source, size_t p_source_size)
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
		default:					return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

EidosValue_SP EidosTestElement::ExecuteMethod_Accelerated_cubicYolk(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
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
	EidosTestElement *sq_element = new EidosTestElement(yolk_ * yolk_);
	EidosValue_SP retval(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(sq_element, gEidosTestElement_Class));
	sq_element->Release();	// retval now owns it; release the retain from new
	
	return retval;
}


//
//	Object instantiation
//
#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	(object<_TestElement>$)_Test(integer$ yolk)
static EidosValue_SP Eidos_Instantiate_EidosTestElement(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *yolk_value = p_arguments[0].get();
	EidosTestElement *objectElement = new EidosTestElement(yolk_value->IntAtIndex(0, nullptr));
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosTestElement_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}


//
//	EidosTestElement_Class
//
#pragma mark -
#pragma mark EidosTestElement_Class
#pragma mark -

EidosClass *gEidosTestElement_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *EidosTestElement_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosTestElement_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
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
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosTestElement_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__cubicYolk, kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedImp(EidosTestElement::ExecuteMethod_Accelerated_cubicYolk));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__squareTest, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElement_Class)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const std::vector<EidosFunctionSignature_CSP> *EidosTestElement_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosTestElement_Class::Functions(): not warmed up");
		
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_Test", Eidos_Instantiate_EidosTestElement, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElement_Class))->AddInt_S("yolk"));
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}


//
//	EidosTestElementNRR
//
#pragma mark -
#pragma mark EidosTestElementNRR
#pragma mark -

EidosTestElementNRR::EidosTestElementNRR(int64_t p_value) : yolk_(p_value)
{
}

EidosTestElementNRR::~EidosTestElementNRR(void)
{
	//std::cout << "~EidosTestElementNRR : " << yolk_ << std::endl;
}

const EidosClass *EidosTestElementNRR::Class(void) const
{
	return gEidosTestElementNRR_Class;
}

void EidosTestElementNRR::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP EidosTestElementNRR::GetProperty(EidosGlobalStringID p_property_id)
{
	if (p_property_id == gEidosID__yolk)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(yolk_));
	
	// all others, including gID_none
	else
		return super::GetProperty(p_property_id);
}

void EidosTestElementNRR::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	if (p_property_id == gEidosID__yolk)				// ACCELERATED
	{
		yolk_ = p_value.IntAtIndex(0, nullptr);
		return;
	}
	
	// all others, including gID_none
	else
		return super::SetProperty(p_property_id, p_value);
}


//
//	Object instantiation
//
#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	(object<_TestElementNRR>$)_TestNRR(integer$ yolk)
static EidosValue_SP Eidos_Instantiate_EidosTestElementNRR(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *yolk_value = p_arguments[0].get();
	EidosTestElementNRR *objectElement = new EidosTestElementNRR(yolk_value->IntAtIndex(0, nullptr));
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosTestElementNRR_Class));
	
	// Note that since these are not under retain/release, and Eidos has no logic to keep track of them and release them, they just leak
	// This is probably what EidosTestElement::FreeThunks() used to do before EidosTestElement was put under retain/release, so that
	// mechanism will probably need to be revived in order for leak checking to work properly.
	
	return result_SP;
}


//
//	EidosTestElementNRR_Class
//
#pragma mark -
#pragma mark EidosTestElementNRR_Class
#pragma mark -

EidosClass *gEidosTestElementNRR_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *EidosTestElementNRR_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosTestElementNRR_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr__yolk,		false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosFunctionSignature_CSP> *EidosTestElementNRR_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosTestElementNRR_Class::Functions(): not warmed up");
		
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("_TestNRR", Eidos_Instantiate_EidosTestElementNRR, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElementNRR_Class))->AddInt_S("yolk"));
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}



































































