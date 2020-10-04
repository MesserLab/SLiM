//
//  eidos_dictionary.cpp
//  SLiM
//
//  Created by Ben Haller on 9/30/20.
//  Copyright © 2020 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#include "eidos_value.h"				// EidosDictionary is declared here
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"

#include <utility>
#include <algorithm>
#include <vector>


#pragma mark -
#pragma mark EidosDictionary
#pragma mark -

EidosDictionary::EidosDictionary(__attribute__((unused)) const EidosDictionary &p_original) : EidosObjectElement()
{
	// copy hash_symbols_ from p_original; I think this is called only when a Substitution is created from a Mutation
	if (p_original.hash_symbols_)
	{
		hash_symbols_ = new std::unordered_map<std::string, EidosValue_SP>;
		
		*hash_symbols_ = *(p_original.hash_symbols_);
	}
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *EidosDictionary::Class(void) const
{
	return gEidos_EidosDictionary_Class;
}

EidosValue_SP EidosDictionary::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gEidosID_getValue:		return ExecuteMethod_getValue(p_method_id, p_arguments, p_interpreter);
		//case gEidosID_setValue:	return ExecuteMethod_Accelerated_setValue(p_method_id, p_arguments, p_interpreter);
		default:					return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (*)getValue(string $key)
//
EidosValue_SP EidosDictionary::ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
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

//	*********************	- (void)setValue(string $key, * value)
//
EidosValue_SP EidosDictionary::ExecuteMethod_Accelerated_setValue(EidosObjectElement **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *key_value = p_arguments[0].get();
	std::string key = key_value->StringAtIndex(0, nullptr);
	EidosValue_SP value = p_arguments[1];
	EidosValueType value_type = value->Type();
	
	// Object values can only be remembered if their class is under retain/release, so that we have control over the object lifetime
	// See also Eidos_ExecuteFunction_defineConstant(), which enforces the same rule
	if (value_type == EidosValueType::kValueObject)
	{
		const EidosObjectClass *value_class = ((EidosValue_Object *)value.get())->Class();
		
		if (!value_class->UsesRetainRelease())
			EIDOS_TERMINATION << "ERROR (EidosDictionary::ExecuteMethod_Accelerated_setValue): setValue() can only accept object classes that are under retain/release memory management internally; class " << value_class->ElementType() << "is not.  This restriction is necessary in order to guarantee that the kept object elements remain valid." << EidosTerminate(nullptr);
	}
	
	if (value_type == EidosValueType::kValueNULL)
	{
		// Setting a key to NULL removes it from the map
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
		{
			EidosDictionary *element = (EidosDictionary *)(p_elements[element_index]);
			
			if (element->hash_symbols_)
				element->hash_symbols_->erase(key);
		}
	}
	else
	{
		// BCH: Copy values just as EidosSymbolTable does, to prevent them from being modified underneath us etc.
		// Note that when setting a value across multiple object targets, however, they all receive the same copy.
		// That should be safe; there should be no way for that value to get modified after we have copied it.  1/28/2019
		// If we have the only reference to the value, we don't need to copy it; otherwise we copy, since we don't want to hold
		// onto a reference that somebody else might modify under us (or that we might modify under them, with syntaxes like
		// x[2]=...; and x=x+1;). If the value is invisible then we copy it, since the symbol table never stores invisible values.
		if ((value->UseCount() != 1) || value->Invisible())
			value = value->CopyValues();
		
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
		{
			EidosDictionary *element = (EidosDictionary *)(p_elements[element_index]);
			
			if (!element->hash_symbols_)
				element->hash_symbols_ = new std::unordered_map<std::string, EidosValue_SP>;
			
			(*element->hash_symbols_)[key] = value;
		}
	}
	
	return gStaticEidosValueVOID;
}


//
//	EidosDictionary_Class
//
#pragma mark -
#pragma mark EidosDictionary_Class
#pragma mark -

EidosObjectClass *gEidos_EidosDictionary_Class = new EidosDictionary_Class();


const std::string &EidosDictionary_Class::ElementType(void) const
{
	return gEidosStr_EidosDictionary;
}

const std::vector<EidosMethodSignature_CSP> *EidosDictionary_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*EidosObjectClass::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_getValue, kEidosValueMaskAny))->AddString_S("key"));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_setValue, kEidosValueMaskVOID))->AddString_S("key")->AddAny("value"))->DeclareAcceleratedImp(EidosDictionary::ExecuteMethod_Accelerated_setValue));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}












































