//
//  eidos_class_Object.cpp
//  Eidos
//
//  Created by Ben Haller on 10/12/20.
//  Copyright (c) 2020-2023 Philipp Messer.  All rights reserved.
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


#include "eidos_class_Object.h"

#include <string>
#include <algorithm>
#include <vector>

#include "eidos_interpreter.h"
#include "eidos_value.h"
#include "eidos_class_Object.h"
#include "eidos_class_Dictionary.h"
#include "eidos_class_DataFrame.h"
#include "eidos_class_Image.h"
#include "eidos_class_TestElement.h"
#include "json.hpp"


//
//	EidosObject
//
#pragma mark -
#pragma mark EidosObject
#pragma mark -

// EidosObject::Class() is not defined, to make this an abstract base class; but of course its class is gEidosObject_Class
/*const EidosClass *EidosObject::Class(void) const
{
	return gEidosObject_Class;
}*/

const EidosClass *EidosObject::Superclass(void) const
{
	const EidosClass *class_object = Class();
	const EidosClass *superclass_object = class_object ? class_object->Superclass() : nullptr;
	
	return superclass_object;
}

bool EidosObject::IsKindOfClass(const EidosClass *p_class_object) const
{
	const EidosClass *class_object = Class();
	
	do
	{
		if (class_object == p_class_object)
			return true;
		
		class_object = class_object->Superclass();
	}
	while (class_object);
	
	return false;
}

bool EidosObject::IsMemberOfClass(const EidosClass *p_class_object) const
{
	return (Class() == p_class_object);
}

void EidosObject::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName();
}

nlohmann::json EidosObject::JSONRepresentation(void) const
{
	// undefined, raises; subclass that know how to serialize themselves can override
	EIDOS_TERMINATION << "ERROR (EidosObject::JSONRepresentation): objects, apart from Dictionary objects, cannot be converted to JSON." << EidosTerminate(nullptr);
}

EidosValue_SP EidosObject::GetProperty(EidosGlobalStringID p_property_id)
{
	// This is the backstop, called by subclasses
	EIDOS_TERMINATION << "ERROR (EidosObject::GetProperty for " << Class()->ClassName() << "): attempt to get a value for property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObject::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
#pragma unused(p_value)
	// This is the backstop, called by subclasses
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObject::SetProperty): property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " is not defined for object element type " << Class()->ClassName() << "." << EidosTerminate(nullptr);
	
	bool readonly = signature->read_only_;
	
	// Check whether setting a constant was attempted; we can do this on behalf of all our subclasses
	if (readonly)
		EIDOS_TERMINATION << "ERROR (EidosObject::SetProperty for " << Class()->ClassName() << "): attempt to set a new value for read-only property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << "." << EidosTerminate(nullptr);
	else
		EIDOS_TERMINATION << "ERROR (EidosObject::SetProperty for " << Class()->ClassName() << "): (internal error) setting a new value for read-write property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

EidosValue_SP EidosObject::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_interpreter)
	switch (p_method_id)
	{
		case gEidosID_str:					return ExecuteMethod_str(p_method_id, p_arguments, p_interpreter);
		case gEidosID_stringRepresentation:	return ExecuteMethod_stringRepresentation(p_method_id, p_arguments, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<EidosMethodSignature_CSP> *methods = Class()->Methods();
			const std::string &method_name = EidosStringRegistry::StringForGlobalStringID(p_method_id);
			
			for (const EidosMethodSignature_CSP &method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObject::ExecuteInstanceMethod for " << Class()->ClassName() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObject::ExecuteInstanceMethod for " << Class()->ClassName() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
		}
	}
}

//	*********************	– (void)str(void)
//
EidosValue_SP EidosObject::ExecuteMethod_str(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << Class()->ClassName() << ":" << std::endl;
	
	const std::vector<EidosPropertySignature_CSP> *properties = Class()->Properties();
	
	for (const EidosPropertySignature_CSP &property_sig : *properties)
	{
		const std::string &property_name = property_sig->property_name_;
		EidosGlobalStringID property_id = property_sig->property_id_;
		EidosValue_SP property_value;
		bool oldSuppressWarnings = gEidosSuppressWarnings;
		
		gEidosSuppressWarnings = true;		// prevent warnings from questionable property accesses from producing warnings in the user's output pane
		
		try {
			property_value = GetProperty(property_id);
		} catch (...) {
			// throw away the raise message
			gEidosTermination.clear();
			gEidosTermination.str(gEidosStr_empty_string);
		}
		
		gEidosSuppressWarnings = oldSuppressWarnings;
		
		if (property_value)
		{
			EidosValueType property_type = property_value->Type();
			int property_count = property_value->Count();
			int property_dimcount = property_value->DimensionCount();
			const int64_t *property_dims = property_value->Dimensions();
			
			output_stream << "\t" << property_name << " " << property_sig->PropertySymbol() << " ";
			
			if (property_count == 0)
			{
				// zero-length vectors get printed according to the standard code in EidosValue
				property_value->Print(output_stream);
			}
			else
			{
				// start with the type, and then the class for object-type values
				output_stream << property_type;
				
				if (property_type == EidosValueType::kValueObject)
					output_stream << "<" << property_value->ElementType() << ">";
				
				// then print the ranges for each dimension
				output_stream << " [";
				
				if (property_dimcount == 1)
					output_stream << "0:" << (property_count - 1) << "] ";
				else
				{
					for (int dim_index = 0; dim_index < property_dimcount; ++dim_index)
					{
						if (dim_index > 0)
							output_stream << ", ";
						output_stream << "0:" << (property_dims[dim_index] - 1);
					}
					
					output_stream << "] ";
				}
				
				// finally, print up to two values, if available, followed by an ellipsis if not all values were printed
				int output_count = std::min(2, property_count);
				
				for (int output_index = 0; output_index < output_count; ++output_index)
				{
					EidosValue_SP value = property_value->GetValueAtIndex(output_index, nullptr);
					
					if (output_index > 0)
						output_stream << gEidosStr_space_string;
					
					output_stream << *value;
				}
				
				if (property_count > output_count)
					output_stream << " ...";
			}
			
			output_stream << std::endl;
		}
		else
		{
			// The property threw an error when we tried to access it, which is allowed
			// for properties that are only valid in specific circumstances
			output_stream << "\t" << property_name << " " << property_sig->PropertySymbol() << " <inaccessible>" << std::endl;
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	– (string$)stringRepresentation(void)
//
EidosValue_SP EidosObject::ExecuteMethod_stringRepresentation(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	std::ostringstream oss;
	
	Print(oss);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(oss.str()));
}

EidosValue_SP EidosObject::ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_function_name, p_arguments, p_interpreter)
	EIDOS_TERMINATION << "ERROR (EidosObject::ContextDefinedFunctionDispatch for " << Class()->ClassName() << "): (internal error) unimplemented Context function dispatch." << EidosTerminate(nullptr);
}

std::ostream &operator<<(std::ostream &p_outstream, const EidosObject &p_element)
{
	p_element.Print(p_outstream);	// get dynamic dispatch
	
	return p_outstream;
}


//
//	EidosClass
//
#pragma mark -
#pragma mark EidosClass
#pragma mark -

// global class registry; this non-const accessor is private
std::vector<EidosClass *> &EidosClass::EidosClassRegistry(void)
{
	// Our global registry is handled this way, so that we don't run into order-of-initialization issues
	static std::vector<EidosClass *> *classRegistry = nullptr;
	
	if (!classRegistry)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosClass::EidosClassRegistry(): not warmed up");
		
		classRegistry = new std::vector<EidosClass *>;
	}
	
	return *classRegistry;
}

std::vector<EidosClass *> EidosClass::RegisteredClasses(bool p_builtin, bool p_context)
{
	// EidosClass::EidosClassRegistry() contains every class that has an allocated EidosClass object; we want to filter that
	std::vector<EidosClass *> &classRegistry = EidosClass::EidosClassRegistry();
	
	if (classRegistry.size() == 0)
		std::cout << "EidosClass::RegisteredClasses() called before classes have been registered" << std::endl;
	
	std::vector<EidosClass *> filteredRegistry;
	
	for (EidosClass *class_object : classRegistry)
	{
		bool builtin = false;
		
		// It's unfortunate to have to hard-code this here, but I can't think of a better
		// heuristic that wouldn't hard-code some detail about SLiM here...
		if ((class_object == gEidosObject_Class) ||
			(class_object == gEidosTestElement_Class) ||
			(class_object == gEidosTestElementNRR_Class) ||
			(class_object == gEidosDictionaryUnretained_Class) ||
			(class_object == gEidosDictionaryRetained_Class) ||
			(class_object == gEidosDataFrame_Class) ||
			(class_object == gEidosImage_Class))
			builtin = true;
		
		if ((builtin && p_builtin) || (!builtin && p_context))
			filteredRegistry.emplace_back(class_object);
	}
	
	return filteredRegistry;
}

std::vector<EidosPropertySignature_CSP> EidosClass::RegisteredClassProperties(bool p_builtin, bool p_context)
{
	std::vector<EidosPropertySignature_CSP> propertySignatures;
	std::vector<EidosClass *> classRegistry = EidosClass::RegisteredClasses(p_builtin, p_context);
	
	for (EidosClass *class_object : classRegistry)
	{
		const std::vector<EidosPropertySignature_CSP> *properties = class_object->Properties();
		
		propertySignatures.insert(propertySignatures.end(), properties->begin(), properties->end());
	}
	
	// sort by pointer; we want pointer-identical signatures to end up adjacent
	std::sort(propertySignatures.begin(), propertySignatures.end());
	
	// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
	auto unique_end_iter = std::unique(propertySignatures.begin(), propertySignatures.end());
	propertySignatures.resize(std::distance(propertySignatures.begin(), unique_end_iter));
	
	// now sort by name
	std::sort(propertySignatures.begin(), propertySignatures.end(), CompareEidosPropertySignatures);
	
	return propertySignatures;
}

std::vector<EidosMethodSignature_CSP> EidosClass::RegisteredClassMethods(bool p_builtin, bool p_context)
{
	std::vector<EidosMethodSignature_CSP> methodSignatures;
	std::vector<EidosClass *> classRegistry = EidosClass::RegisteredClasses(p_builtin, p_context);
	
	for (EidosClass *class_object : classRegistry)
	{
		const std::vector<EidosMethodSignature_CSP> *methods = class_object->Methods();
		
		methodSignatures.insert(methodSignatures.end(), methods->begin(), methods->end());
	}
	
	// sort by pointer; we want pointer-identical signatures to end up adjacent
	std::sort(methodSignatures.begin(), methodSignatures.end());
	
	// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
	auto unique_end_iter = std::unique(methodSignatures.begin(), methodSignatures.end());
	methodSignatures.resize(std::distance(methodSignatures.begin(), unique_end_iter));
	
	// now sort by name
	std::sort(methodSignatures.begin(), methodSignatures.end(), CompareEidosCallSignatures);
	
	return methodSignatures;
}

void EidosClass::CheckForDuplicateMethodsOrProperties(void)
{
	std::vector<EidosPropertySignature_CSP> all_properties = RegisteredClassProperties(true, true);
	std::vector<EidosMethodSignature_CSP> all_methods = RegisteredClassMethods(true, true);
	
	// print out any property signatures that are identical by name but are not identical
	{
		EidosPropertySignature_CSP previous_sig = nullptr;
		
		for (const EidosPropertySignature_CSP &sig : all_properties)
		{
			if (previous_sig && (sig->property_name_.compare(previous_sig->property_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the property signatures are identical.
				if ((sig->property_id_ != previous_sig->property_id_) ||
					(sig->read_only_ != previous_sig->read_only_) ||
					(sig->value_mask_ != previous_sig->value_mask_) ||
					(sig->value_class_ != previous_sig->value_class_))
					std::cout << "Duplicate property name with different signature: " << sig->property_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	// print out any method signatures that are identical by name but are not identical
	{
		EidosMethodSignature_CSP previous_sig = nullptr;
		
		for (const EidosMethodSignature_CSP &sig : all_methods)
		{
			if (previous_sig && (sig->call_name_.compare(previous_sig->call_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the method signatures are identical.
				const EidosMethodSignature *sig1 = sig.get();
				const EidosMethodSignature *sig2 = previous_sig.get();
				
				if ((typeid(*sig1) != typeid(*sig2)) ||
					(sig->is_class_method != previous_sig->is_class_method) ||
					(sig->call_name_ != previous_sig->call_name_) ||
					(sig->return_mask_ != previous_sig->return_mask_) ||
					(sig->return_class_ != previous_sig->return_class_) ||
					(sig->arg_masks_ != previous_sig->arg_masks_) ||
					(sig->arg_names_ != previous_sig->arg_names_) ||
					(sig->arg_classes_ != previous_sig->arg_classes_) ||
					(sig->has_optional_args_ != previous_sig->has_optional_args_) ||
					(sig->has_ellipsis_ != previous_sig->has_ellipsis_))
					std::cout << "Duplicate method name with a different signature: " << sig->call_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
}

EidosClass::EidosClass(const std::string &p_class_name, EidosClass *p_superclass) : class_name_(p_class_name), superclass_(p_superclass)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("EidosClass::EidosClass(): not warmed up");
	
	// Every EidosClass instance gets added to a shared registry, so that Eidos can find them all
	EidosClassRegistry().emplace_back(this);
}

EidosClass::~EidosClass(void)
{
}

bool EidosClass::UsesRetainRelease(void) const
{
	return false;
}

bool EidosClass::IsSubclassOfClass(const EidosClass *p_class_object) const
{
	const EidosClass *class_object = this;
	
	do
	{
		if (class_object == p_class_object)
			return true;
		
		class_object = class_object->Superclass();
	}
	while (class_object);
	
	return false;
}

void EidosClass::CacheDispatchTables(void)
{
	// This can be called more than once during startup, because Eidos warms up and the SLiM warms up
	if (dispatches_cached_)
		return;
	
	{
		const std::vector<EidosPropertySignature_CSP> *properties = Properties();
		int32_t last_id = -1;
		
		for (const EidosPropertySignature_CSP &sig : *properties)
			last_id = std::max(last_id, (int32_t)sig->property_id_);
		
		property_signatures_dispatch_capacity_ = last_id + 1;
		
		// this limit may need to be lifted someday, but for now it's a sanity check if the uniquing code changes
		// all properties should use explicitly registered strings, in the present design, so they should all be <= gEidosID_LastContextEntry
		if ((int64_t)property_signatures_dispatch_capacity_ > (int64_t)gEidosID_LastContextEntry)
			EIDOS_TERMINATION << "ERROR (EidosClass::CacheDispatchTables): (internal error) property dispatch table unreasonably large for class " << ClassName() << "." << EidosTerminate(nullptr);
		
		property_signatures_dispatch_ = (EidosPropertySignature_CSP *)calloc(property_signatures_dispatch_capacity_, sizeof(EidosPropertySignature_CSP));
		if (!property_signatures_dispatch_)
			EIDOS_TERMINATION << "ERROR (EidosClass::CacheDispatchTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		for (const EidosPropertySignature_CSP &sig : *properties)
			property_signatures_dispatch_[sig->property_id_] = sig;
	}
	
	{
		const std::vector<EidosMethodSignature_CSP> *methods = Methods();
		int32_t last_id = -1;
		
		for (const EidosMethodSignature_CSP &sig : *methods)
			last_id = std::max(last_id, (int32_t)sig->call_id_);
		
		method_signatures_dispatch_capacity_ = last_id + 1;
		
		// this limit may need to be lifted someday, but for now it's a sanity check if the uniquing code changes
		if (method_signatures_dispatch_capacity_ > 512)
			EIDOS_TERMINATION << "ERROR (EidosClass::CacheDispatchTables): (internal error) method dispatch table unreasonably large for class " << ClassName() << "." << EidosTerminate(nullptr);
		
		method_signatures_dispatch_ = (EidosMethodSignature_CSP *)calloc(method_signatures_dispatch_capacity_, sizeof(EidosMethodSignature_CSP));
		if (!method_signatures_dispatch_)
			EIDOS_TERMINATION << "ERROR (EidosClass::CacheDispatchTables): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		for (const EidosMethodSignature_CSP &sig : *methods)
			method_signatures_dispatch_[sig->call_id_] = sig;
	}
	
	//std::cout << ElementType() << " property/method lookup table capacities: " << property_signatures_dispatch_capacity_ << " / " << method_signatures_dispatch_capacity_ << " (" << ((property_signatures_dispatch_capacity_ + method_signatures_dispatch_capacity_) * sizeof(EidosPropertySignature *)) << " bytes)" << std::endl;
	
	dispatches_cached_ = true;
}

void EidosClass::RaiseForDispatchUninitialized(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosClass::RaiseForDispatchUninitialized): (internal error) dispatch tables not initialized for class " << ClassName() << "." << EidosTerminate(nullptr);
}

const std::vector<EidosPropertySignature_CSP> *EidosClass::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosClass::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>;
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosClass::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosClass::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>;
		
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_methodSignature, kEidosValueMaskVOID))->AddString_OSN("methodName", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_propertySignature, kEidosValueMaskVOID))->AddString_OSN("propertyName", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_size, kEidosValueMaskInt | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_length, kEidosValueMaskInt | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_str, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_stringRepresentation, kEidosValueMaskString | kEidosValueMaskSingleton)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const std::vector<EidosFunctionSignature_CSP> *EidosClass::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("EidosClass::Functions(): not warmed up");
		
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}

EidosValue_SP EidosClass::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gEidosID_propertySignature:	return ExecuteMethod_propertySignature(p_method_id, p_target, p_arguments, p_interpreter);
		case gEidosID_methodSignature:		return ExecuteMethod_methodSignature(p_method_id, p_target, p_arguments, p_interpreter);
		case gEidosID_size:					return ExecuteMethod_size_length(p_method_id, p_target, p_arguments, p_interpreter);		// NOLINT(*-branch-clone) : intentional consecutive branches
		case gEidosID_length:				return ExecuteMethod_size_length(p_method_id, p_target, p_arguments, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<EidosMethodSignature_CSP> *methods = Methods();
			const std::string &method_name = EidosStringRegistry::StringForGlobalStringID(p_method_id);
			
			for (const EidosMethodSignature_CSP &method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosClass::ExecuteClassMethod for " << ClassName() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosClass::ExecuteClassMethod for " << ClassName() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
		}
	}
}

//	*********************	+ (void)propertySignature([Ns$ propertyName = NULL])
//
EidosValue_SP EidosClass::ExecuteMethod_propertySignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
	EidosValue_String *propertyName_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &match_string = (has_match_string ? propertyName_value->StringRefAtIndex(0, nullptr) : gEidosStr_empty_string);
	const std::vector<EidosPropertySignature_CSP> *properties = Properties();
	bool signature_found = false;
	
	for (const EidosPropertySignature_CSP &property_sig : *properties)
	{
		const std::string &property_name = property_sig->property_name_;
		
		if (has_match_string && (property_name.compare(match_string) != 0))
			continue;
		
		output_stream << property_name << " " << property_sig->PropertySymbol() << " (" << StringForEidosValueMask(property_sig->value_mask_, property_sig->value_class_, "", nullptr) << ")" << std::endl;
		
		signature_found = true;
	}
	
	if (has_match_string && !signature_found)
		output_stream << "No property found for '" << match_string << "'." << std::endl;
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (void)methodSignature([Ns$ methodName = NULL])
//
EidosValue_SP EidosClass::ExecuteMethod_methodSignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
	EidosValue_String *methodName_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &match_string = (has_match_string ? methodName_value->StringRefAtIndex(0, nullptr) : gEidosStr_empty_string);
	const std::vector<EidosMethodSignature_CSP> *methods = Methods();
	bool signature_found = false;
	
	// Output class methods first
	for (const EidosMethodSignature_CSP &method_sig : *methods)
	{
		if (!method_sig->is_class_method)
			continue;
		
		const std::string &method_name = method_sig->call_name_;
		
		if (has_match_string && (method_name.compare(match_string) != 0))
			continue;
		
		output_stream << *method_sig << std::endl;
		signature_found = true;
	}
	
	// Then instance methods
	for (const EidosMethodSignature_CSP &method_sig : *methods)
	{
		if (method_sig->is_class_method)
			continue;
		
		const std::string &method_name = method_sig->call_name_;
		
		if (has_match_string && (method_name.compare(match_string) != 0))
			continue;
		
		output_stream << *method_sig << std::endl;
		signature_found = true;
	}
	
	if (has_match_string && !signature_found)
		output_stream << "No method signature found for '" << match_string << "'." << std::endl;
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (integer$)size(void)
//	*********************	+ (integer$)length(void)
//
EidosValue_SP EidosClass::ExecuteMethod_size_length(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(p_target->Count()));
}


































