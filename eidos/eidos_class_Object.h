//
//  eidos_class_Object.h
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

#ifndef __Eidos__eidos_class_Object__
#define __Eidos__eidos_class_Object__


#include <iostream>
#include <vector>
#include <string>

#include "eidos_globals.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"
#include "json_fwd.hpp"

class EidosClass;

#ifdef SLIMGUI
// Forward declaration of EidosInterpreterDebugPointsSet; see eidos_interpreter.cpp
struct EidosInterpreterDebugPointsSet_struct;
typedef EidosInterpreterDebugPointsSet_struct EidosInterpreterDebugPointsSet;
#endif


#pragma mark -
#pragma mark EidosObject
#pragma mark -

//	*********************************************************************************************************
//
// This is the value type of which EidosValue_Object is a vector, just as double is the value type of which
// EidosValue_Float is a vector.  EidosValue_Object is just a bag; this class is the abstract base class of
// the things that can be contained in that bag.  This class defines the methods that can be used by an
// instance of EidosValue_Object; EidosValue_Object just forwards such things on to this class.
// EidosObject obeys sharing semantics; many EidosValue_Objects can refer to the same element, and
// EidosObject never copies itself.  To manage its lifetime, refcounting can be used.  Many objects
// do not use this refcount, since their lifetime is managed, but some objects, such as Mutation and the
// internal test class EidosTestElement, use the refcount and delete themselves when they are done.  Those
// objects inherit from EidosDictionaryRetained, and their EidosClass subclass must subclass from
// EidosDictionaryRetained_Class (which guarantees inheritance from EidosDictionaryRetained).

class EidosObject
{
public:
	EidosObject(const EidosObject &p_original) = delete;		// no copy-construct
	EidosObject& operator=(const EidosObject&) = delete;		// no copying
	
	inline EidosObject(void) { }
	inline virtual ~EidosObject(void) { }
	
	virtual const EidosClass *Class(void) const = 0;
	const EidosClass *Superclass(void) const;
	bool IsKindOfClass(const EidosClass *p_class_object) const;
	bool IsMemberOfClass(const EidosClass *p_class_object) const;
	
	virtual void Print(std::ostream &p_ostream) const;		// standard printing; prints Class()->ClassName()
	virtual nlohmann::json JSONRepresentation(void) const;	// undefined, raises; subclass that know how to serialize themselves can override
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_str(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_stringRepresentation(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// EidosContext is a typedef for EidosObject at present, so this class is the superclass of the Context
	// object.  If that gets complicated we'll probably want to make a new EidosContext class to formalize things,
	// but for now the only addition we need for that is this virtual function stub, used for Context-defined
	// function dispatch.
	virtual EidosValue_SP ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
#ifdef SLIMGUI
	// Because this class is used for the Eidos Context, stuff related to the context has to be defined here.
	// We don't want to dictate what subclass of EidosObject the context might actually inherit from, so we
	// can't define this in a subclass.  I wish we had Obj-C protocols...
	
	// Support for debugging points in SLiMgui
	virtual EidosInterpreterDebugPointsSet *DebugPoints(void) { return nullptr; };
	virtual std::string DebugPointInfo(void) { return ""; };
	
	// Support for monitoring of file I/O in SLiMgui
	virtual void FileWriteNotification(__attribute__((unused)) const std::string &p_file_path, __attribute__((unused)) std::vector<std::string> &&p_lines, __attribute__((unused)) bool p_append) { ; };
#endif
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosObject &p_element);


#pragma mark -
#pragma mark EidosClass
#pragma mark -

//	*********************************************************************************************************
//
// This class is similar to Class objects in Objective-C; it represents the class of an EidosObject.
// These class objects are not visible in Eidos, at least at present; they just work behind the scenes to
// enable some behaviors.  In particular: (1) class objects define the interface, of methods and properties,
// that elements implement; (2) class objects implement class methods, avoiding the need to call class
// methods on an arbitrarily chosen instance of an element type; and most importantly, (3) class objects exist
// even when no instance exists, allowing facilities like code completion to handle types and interfaces
// without having to have instantiated object elements in hand.  It is conceivable that class objects will
// become visible in Eidos at some point; but there seems to be no need for that at present.
//
// Note that this is not an abstract base class!  It is used to represent the class of empty object vectors.

extern EidosClass *gEidosObject_Class;

class EidosClass
{
private:
	// This is the element type for vectors of this class.  Note that it is a reference to a std::string that
	// is defined externally, which must continue to exist for the lifetime of this class object.  This is
	// so that we can use pointer equality on class names for speed, notably in takeMigrants().
	const std::string &class_name_;
	
	// This is the EidosClass object representing our superclass
	EidosClass *superclass_;
	
	static std::vector<EidosClass *> &EidosClassRegistry(void);

protected:
	// cached dispatch tables; these are lookup tables, indexed by EidosGlobalStringID property / method ids
	bool dispatches_cached_ = false;
	
	EidosPropertySignature_CSP *property_signatures_dispatch_ = nullptr;
	int32_t property_signatures_dispatch_capacity_ = 0;
	
	EidosMethodSignature_CSP *method_signatures_dispatch_ = nullptr;
	int32_t method_signatures_dispatch_capacity_ = 0;
	
public:
	static std::vector<EidosClass *> RegisteredClasses(bool p_builtin, bool p_context);
	static std::vector<EidosPropertySignature_CSP> RegisteredClassProperties(bool p_builtin, bool p_context);
	static std::vector<EidosMethodSignature_CSP> RegisteredClassMethods(bool p_builtin, bool p_context);
	static void CheckForDuplicateMethodsOrProperties(void);
	
	EidosClass(const EidosClass &p_original) = delete;		// no copy-construct
	EidosClass& operator=(const EidosClass&) = delete;		// no copying
	
	EidosClass(const std::string &p_class_name, EidosClass *p_superclass);
	virtual ~EidosClass(void);
	
	virtual bool UsesRetainRelease(void) const;
	
	inline const EidosClass *Class(void) const { return this; }
	inline const std::string &ClassName(void) const { return class_name_; };
	inline const EidosClass *Superclass(void) const { return superclass_; }
	bool IsSubclassOfClass(const EidosClass *p_class_object) const;
	
	// We now use dispatch tables to look up our property and method signatures; this is faster than the old switch() dispatch
	void CacheDispatchTables(void);
	void RaiseForDispatchUninitialized(void) const __attribute__((__noreturn__)) __attribute__((analyzer_noreturn));
	
	inline __attribute__((always_inline)) const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const
	{
#if DEBUG
		if (!dispatches_cached_)
			RaiseForDispatchUninitialized();
#endif
		if (p_property_id < (EidosGlobalStringID)property_signatures_dispatch_capacity_)
			return property_signatures_dispatch_[p_property_id].get();	// the assumption is short-term use by the caller
		
		return nullptr;
	}
	
	inline __attribute__((always_inline)) const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const
	{
#if DEBUG
		if (!dispatches_cached_)
			RaiseForDispatchUninitialized();
#endif
		if (p_method_id < (EidosGlobalStringID)method_signatures_dispatch_capacity_)
			return method_signatures_dispatch_[p_method_id].get();	// the assumption is short-term use by the caller
		
		return nullptr;
	}
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const;
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_propertySignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_methodSignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_size_length(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const;
};


#endif /* __Eidos__eidos_class_Object__ */







































