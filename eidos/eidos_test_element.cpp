//
//  eidos_test_element.cpp
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
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


using std::string;
using std::vector;
using std::endl;


//
//	Eidos_TestElement
//
#pragma mark Eidos_TestElement

Eidos_TestElement::Eidos_TestElement(int64_t p_value) : yolk_(p_value)
{
}

const EidosObjectClass *Eidos_TestElement::Class(void) const
{
	return gEidos_TestElementClass;
}

EidosValue *Eidos_TestElement::GetProperty(EidosGlobalStringID p_property_id)
{
	if (p_property_id == gEidosID__yolk)
		return new EidosValue_Int_singleton_const(yolk_);
	else if (p_property_id == gEidosID__increment)
		return new EidosValue_Object_singleton_const(new Eidos_TestElement(yolk_ + 1));
	
	// all others, including gID_none
	else
		return EidosObjectElement::GetProperty(p_property_id);
}

void Eidos_TestElement::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	if (p_property_id == gEidosID__yolk)
	{
		yolk_ = p_value->IntAtIndex(0);
		return;
	}
	
	// all others, including gID_none
	else
		return EidosObjectElement::SetProperty(p_property_id, p_value);
}

EidosValue *Eidos_TestElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gEidosID__cubicYolk:
		{
			return new EidosValue_Int_singleton_const(yolk_ * yolk_ * yolk_);
		}
		case gEidosID__squareTest:
		{
			return new EidosValue_Object_singleton_const(new Eidos_TestElement(yolk_ * yolk_));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	Eidos_TestElementClass
//
#pragma mark -
#pragma mark Eidos_TestElementClass

class Eidos_TestElementClass : public EidosObjectClass
{
public:
	Eidos_TestElementClass(const Eidos_TestElementClass &p_original) = delete;	// no copy-construct
	Eidos_TestElementClass& operator=(const Eidos_TestElementClass&) = delete;	// no copying
	
	Eidos_TestElementClass(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue *ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gEidos_TestElementClass = new Eidos_TestElementClass();


Eidos_TestElementClass::Eidos_TestElementClass(void)
{
}

const std::string &Eidos_TestElementClass::ElementType(void) const
{
	return gEidosStr__TestElement;
}

const std::vector<const EidosPropertySignature *> *Eidos_TestElementClass::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->push_back(SignatureForPropertyOrRaise(gEidosID__yolk));
		properties->push_back(SignatureForPropertyOrRaise(gEidosID__increment));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Eidos_TestElementClass::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *yolkSig = nullptr;
	static EidosPropertySignature *incrementSig = nullptr;
	
	if (!yolkSig)
	{
		yolkSig =		(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr__yolk,		gEidosID__yolk,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		incrementSig =	(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr__increment,	gEidosID__increment,	true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gEidos_TestElementClass));
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

const std::vector<const EidosMethodSignature *> *Eidos_TestElementClass::Methods(void) const
{
	std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->push_back(SignatureForMethodOrRaise(gEidosID__cubicYolk));
		methods->push_back(SignatureForMethodOrRaise(gEidosID__squareTest));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Eidos_TestElementClass::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *cubicYolkSig = nullptr;
	static EidosInstanceMethodSignature *squareTestSig = nullptr;
	
	if (!cubicYolkSig)
	{
		cubicYolkSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__cubicYolk, kEidosValueMaskInt | kEidosValueMaskSingleton));
		squareTestSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr__squareTest, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidos_TestElementClass));
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

EidosValue *Eidos_TestElementClass::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


































































