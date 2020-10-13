//
//  eidos_class_Object.cpp
//  Eidos
//
//  Created by Ben Haller on 10/12/20.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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

#include "eidos_interpreter.h"
#include "eidos_value.h"


//
//	EidosObject
//
#pragma mark -
#pragma mark EidosObject
#pragma mark -

void EidosObject::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType();
}

EidosValue_SP EidosObject::GetProperty(EidosGlobalStringID p_property_id)
{
	// This is the backstop, called by subclasses
	EIDOS_TERMINATION << "ERROR (EidosObject::GetProperty for " << Class()->ElementType() << "): attempt to get a value for property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

void EidosObject::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
#pragma unused(p_value)
	// This is the backstop, called by subclasses
	const EidosPropertySignature *signature = Class()->SignatureForProperty(p_property_id);
	
	if (!signature)
		EIDOS_TERMINATION << "ERROR (EidosObject::SetProperty): property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " is not defined for object element type " << Class()->ElementType() << "." << EidosTerminate(nullptr);
	
	bool readonly = signature->read_only_;
	
	// Check whether setting a constant was attempted; we can do this on behalf of all our subclasses
	if (readonly)
		EIDOS_TERMINATION << "ERROR (EidosObject::SetProperty for " << Class()->ElementType() << "): attempt to set a new value for read-only property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << "." << EidosTerminate(nullptr);
	else
		EIDOS_TERMINATION << "ERROR (EidosObject::SetProperty for " << Class()->ElementType() << "): (internal error) setting a new value for read-write property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " was not handled by subclass." << EidosTerminate(nullptr);
}

EidosValue_SP EidosObject::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_arguments, p_interpreter)
	switch (p_method_id)
	{
		case gEidosID_str:	return ExecuteMethod_str(p_method_id, p_arguments, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<EidosMethodSignature_CSP> *methods = Class()->Methods();
			const std::string &method_name = EidosStringRegistry::StringForGlobalStringID(p_method_id);
			
			for (const EidosMethodSignature_CSP &method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosObject::ExecuteInstanceMethod for " << Class()->ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosObject::ExecuteInstanceMethod for " << Class()->ElementType() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
		}
	}
}

//	*********************	– (void)str(void)
//
EidosValue_SP EidosObject::ExecuteMethod_str(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << Class()->ElementType() << ":" << std::endl;
	
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

EidosValue_SP EidosObject::ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_function_name, p_arguments, p_interpreter)
	EIDOS_TERMINATION << "ERROR (EidosObject::ContextDefinedFunctionDispatch for " << Class()->ElementType() << "): (internal error) unimplemented Context function dispatch." << EidosTerminate(nullptr);
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

// global class registry; this non-const accessor is private to this file
std::vector<EidosClass *> &EidosClass::EidosClassRegistry(void)
{
	// Our global registry is handled this way, so that we don't run into order-of-initialization issues
	static std::vector<EidosClass *> *classRegistry = nullptr;
	
	if (!classRegistry)
		classRegistry = new std::vector<EidosClass *>;
	
	return *classRegistry;
}

EidosClass::EidosClass(void)
{
	// Every EidosClass instance gets added to a shared registry, so that Eidos can find them all
	EidosClassRegistry().push_back(this);
}

bool EidosClass::UsesRetainRelease(void) const
{
	return false;
}

const std::string &EidosClass::ElementType(void) const
{
	return gEidosStr_undefined;
}

void EidosClass::CacheDispatchTables(void)
{
	if (dispatches_cached_)
		EIDOS_TERMINATION << "ERROR (EidosClass::CacheDispatchTables): (internal error) dispatch tables already initialized for class " << ElementType() << "." << EidosTerminate(nullptr);
	
	{
		const std::vector<EidosPropertySignature_CSP> *properties = Properties();
		int32_t last_id = -1;
		
		for (const EidosPropertySignature_CSP &sig : *properties)
			last_id = std::max(last_id, (int32_t)sig->property_id_);
		
		property_signatures_dispatch_capacity_ = last_id + 1;
		
		// this limit may need to be lifted someday, but for now it's a sanity check if the uniquing code changes
		// all properties should use explicitly registered strings, in the present design, so they should all be <= gEidosID_LastContextEntry
		if ((int64_t)property_signatures_dispatch_capacity_ > (int64_t)gEidosID_LastContextEntry)
			EIDOS_TERMINATION << "ERROR (EidosClass::CacheDispatchTables): (internal error) property dispatch table unreasonably large for class " << ElementType() << "." << EidosTerminate(nullptr);
		
		property_signatures_dispatch_ = (EidosPropertySignature_CSP *)calloc(property_signatures_dispatch_capacity_, sizeof(EidosPropertySignature_CSP));
		
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
			EIDOS_TERMINATION << "ERROR (EidosClass::CacheDispatchTables): (internal error) method dispatch table unreasonably large for class " << ElementType() << "." << EidosTerminate(nullptr);
		
		method_signatures_dispatch_ = (EidosMethodSignature_CSP *)calloc(method_signatures_dispatch_capacity_, sizeof(EidosMethodSignature_CSP));
		
		for (const EidosMethodSignature_CSP &sig : *methods)
			method_signatures_dispatch_[sig->call_id_] = sig;
	}
	
	//std::cout << ElementType() << " property/method lookup table capacities: " << property_signatures_dispatch_capacity_ << " / " << method_signatures_dispatch_capacity_ << " (" << ((property_signatures_dispatch_capacity_ + method_signatures_dispatch_capacity_) * sizeof(EidosPropertySignature *)) << " bytes)" << std::endl;
	
	dispatches_cached_ = true;
}

void EidosClass::RaiseForDispatchUninitialized(void) const
{
	EIDOS_TERMINATION << "ERROR (EidosClass::RaiseForDispatchUninitialized): (internal error) dispatch tables not initialized for class " << ElementType() << "." << EidosTerminate(nullptr);
}

const std::vector<EidosPropertySignature_CSP> *EidosClass::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
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
		methods = new std::vector<EidosMethodSignature_CSP>;
		
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_methodSignature, kEidosValueMaskVOID))->AddString_OSN("methodName", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_propertySignature, kEidosValueMaskVOID))->AddString_OSN("propertyName", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_size, kEidosValueMaskInt | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gEidosStr_length, kEidosValueMaskInt | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_str, kEidosValueMaskVOID)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const std::vector<EidosFunctionSignature_CSP> *EidosClass::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
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
		case gEidosID_size:					return ExecuteMethod_size_length(p_method_id, p_target, p_arguments, p_interpreter);
		case gEidosID_length:				return ExecuteMethod_size_length(p_method_id, p_target, p_arguments, p_interpreter);
			
		default:
		{
			// Check whether the method call failed due to a bad subclass implementation
			const std::vector<EidosMethodSignature_CSP> *methods = Methods();
			const std::string &method_name = EidosStringRegistry::StringForGlobalStringID(p_method_id);
			
			for (const EidosMethodSignature_CSP &method_sig : *methods)
				if (method_sig->call_name_.compare(method_name) == 0)
					EIDOS_TERMINATION << "ERROR (EidosClass::ExecuteClassMethod for " << ElementType() << "): (internal error) method " << method_name << " was not handled by subclass." << EidosTerminate(nullptr);
			
			// Otherwise, we have an unrecognized method, so throw
			EIDOS_TERMINATION << "ERROR (EidosClass::ExecuteClassMethod for " << ElementType() << "): unrecognized method name " << method_name << "." << EidosTerminate(nullptr);
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
	std::string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
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
		output_stream << "No property found for \"" << match_string << "\"." << std::endl;
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (void)methodSignature([Ns$ methodName = NULL])
//
EidosValue_SP EidosClass::ExecuteMethod_methodSignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	bool has_match_string = (p_arguments[0]->Type() == EidosValueType::kValueString);
	std::string match_string = (has_match_string ? p_arguments[0]->StringAtIndex(0, nullptr) : gEidosStr_empty_string);
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
		output_stream << "No method signature found for \"" << match_string << "\"." << std::endl;
	
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


































