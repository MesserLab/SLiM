//
//  eidos_test_element.cpp
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
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


#include "eidos_test_element.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_global.h"

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
	for (auto thunk_iter = inc_element_thunk.begin(); thunk_iter != inc_element_thunk.end(); thunk_iter++)
		delete (*thunk_iter);
	
	inc_element_thunk.clear();
	
	for (auto thunk_iter = sq_element_thunk.begin(); thunk_iter != sq_element_thunk.end(); thunk_iter++)
		delete (*thunk_iter);
	
	sq_element_thunk.clear();
}


//
//	EidosTestElement
//
#pragma mark EidosTestElement

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

int64_t EidosTestElement::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gEidosID__yolk:		return yolk_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

void EidosTestElement::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	if (p_property_id == gEidosID__yolk)
	{
		yolk_ = p_value.IntAtIndex(0, nullptr);
		return;
	}
	
	// all others, including gID_none
	else
		return EidosObjectElement::SetProperty(p_property_id, p_value);
}

EidosValue_SP EidosTestElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID__cubicYolk:	return ExecuteMethod_cubicYolk(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gEidosID__squareTest:	return ExecuteMethod_squareTest(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:					return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

EidosValue_SP EidosTestElement::ExecuteMethod_cubicYolk(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(yolk_ * yolk_ * yolk_));
}

EidosValue_SP EidosTestElement::ExecuteMethod_squareTest(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
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

class EidosTestElement_Class : public EidosObjectClass
{
public:
	EidosTestElement_Class(const EidosTestElement_Class &p_original) = delete;	// no copy-construct
	EidosTestElement_Class& operator=(const EidosTestElement_Class&) = delete;	// no copying
	
	EidosTestElement_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gEidosTestElement_Class = new EidosTestElement_Class();


EidosTestElement_Class::EidosTestElement_Class(void)
{
}

const std::string &EidosTestElement_Class::ElementType(void) const
{
	return gEidosStr__TestElement;
}

const std::vector<const EidosPropertySignature *> *EidosTestElement_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gEidosID__yolk));
		properties->emplace_back(SignatureForPropertyOrRaise(gEidosID__increment));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *EidosTestElement_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *yolkSig = nullptr;
	static EidosPropertySignature *incrementSig = nullptr;
	
	if (!yolkSig)
	{
		yolkSig =		(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr__yolk,		gEidosID__yolk,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		incrementSig =	(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr__increment,	gEidosID__increment,	true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElement_Class));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gEidosID__yolk:		return yolkSig;
		case gEidosID__increment:	return incrementSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *EidosTestElement_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID__cubicYolk));
		methods->emplace_back(SignatureForMethodOrRaise(gEidosID__squareTest));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *EidosTestElement_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *cubicYolkSig = nullptr;
	static EidosInstanceMethodSignature *squareTestSig = nullptr;
	
	if (!cubicYolkSig)
	{
		cubicYolkSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__cubicYolk, kEidosValueMaskInt | kEidosValueMaskSingleton));
		squareTestSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__squareTest, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosTestElement_Class));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID__cubicYolk:	return cubicYolkSig;
		case gEidosID__squareTest:	return squareTestSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP EidosTestElement_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}


































































