//
//  slim_eidos_dictionary.cpp
//  SLiM
//
//  Created by Ben Haller on 12/6/16.
//  Copyright (c) 2016-2017 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "slim_eidos_dictionary.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"
#include "slim_global.h"

#include <utility>
#include <algorithm>
#include <vector>


#pragma mark -
#pragma mark SLiMEidosDictionary
#pragma mark -

SLiMEidosDictionary::SLiMEidosDictionary(__attribute__((unused)) const SLiMEidosDictionary &p_original)
{
	// Note we do NOT copy hash_symbols_ from p_original; I don't think we need to.  I think, in fact,
	// that this constructor is never called, although the STL insists that it has to exist.
}

SLiMEidosDictionary::SLiMEidosDictionary(void)
{
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *SLiMEidosDictionary::Class(void) const
{
	return gSLiM_SLiMEidosDictionary_Class;
}

EidosValue_SP SLiMEidosDictionary::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_getValue:	return ExecuteMethod_getValue(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setValue:	return ExecuteMethod_setValue(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	- (+)getValue(string $key)
//
EidosValue_SP SLiMEidosDictionary::ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *key_value = p_arguments[0].get();
	
	std::string key = key_value->StringAtIndex(0, nullptr);
	
	if (!hash_symbols_)
		return gStaticEidosValueNULL;
	
	auto found_iter = hash_symbols_->find(key);
	
	if (found_iter == hash_symbols_->end())
	{
		return gStaticEidosValueNULL;
	}
	else
	{
		return found_iter->second;
	}
}

//	*********************	- (void)setValue(string $key, + value)
//
EidosValue_SP SLiMEidosDictionary::ExecuteMethod_setValue(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *key_value = p_arguments[0].get();
	
	std::string key = key_value->StringAtIndex(0, nullptr);
	EidosValue_SP value = p_arguments[1];
	EidosValueType value_type = value->Type();
	
	if (value_type == EidosValueType::kValueNULL)
	{
		// Setting a key to NULL removes it from the map
		if (hash_symbols_)
			hash_symbols_->erase(key);
	}
	else
	{
		if (!hash_symbols_)
			hash_symbols_ = new std::unordered_map<std::string, EidosValue_SP>;
		
		(*hash_symbols_)[key] = std::move(value);
	}
	
	return gStaticEidosValueNULLInvisible;
}


//
//	SLiMEidosDictionary_Class
//
#pragma mark -
#pragma mark SLiMEidosDictionary_Class
#pragma mark -

EidosObjectClass *gSLiM_SLiMEidosDictionary_Class = new SLiMEidosDictionary_Class();


SLiMEidosDictionary_Class::SLiMEidosDictionary_Class(void)
{
}

const std::string &SLiMEidosDictionary_Class::ElementType(void) const
{
	return gStr_SLiMEidosDictionary;
}

const std::vector<const EidosMethodSignature *> *SLiMEidosDictionary_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_getValue));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setValue));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *SLiMEidosDictionary_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *getValueSig = nullptr;
	static EidosInstanceMethodSignature *setValueSig = nullptr;
	
	if (!getValueSig)
	{
		getValueSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_getValue, kEidosValueMaskAnyBase))->AddString_S("key");
		setValueSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setValue, kEidosValueMaskNULL))->AddString_S("key")->AddAnyBase("value");
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_getValue:			return getValueSig;
		case gID_setValue:			return setValueSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}












































