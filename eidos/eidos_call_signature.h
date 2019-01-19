//
//  eidos_function_signature.h
//  Eidos
//
//  Created by Ben Haller on 5/16/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
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


#ifndef __Eidos__eidos_function_signature__
#define __Eidos__eidos_function_signature__

#include "eidos_value.h"

#include <iostream>
#include <memory>


// Prototype for a function handler that is internal to Eidos.
typedef EidosValue_SP (*EidosInternalFunctionPtr)(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);

// This typedef is for an "accelerated method implementation"  These are static member functions on a class, designed to provide
// a whole vector of results given a buffer of EidosObjectElements.  The imp is expected to return the correct type for the method.
// The imp is guaranteed that the EidosObjectElements are of the correct class; it is allowed to do a cast of p_values directly to
// its own type without checking, according to the calling conventions used here.  There are similarities between this and making
// a method a class method, but class methods are conceptually called just once and produce one result from the whole call,
// whereas accelerated method implementations are conceptually called once per element and produce one result per element; they
// are just implemented internally in a vectorized fashion.
typedef EidosValue_SP (*Eidos_AcceleratedMethodImp)(EidosObjectElement **p_values, size_t p_values_size, EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark EidosCallSignature
#pragma mark -

// EidosCallSignature is used to represent the return type and argument types of a function or method, for shared runtime type checking.
// This is an abstract base class; clients should use one of the subclasses below.
class EidosCallSignature
{
public:
	std::string call_name_;
	EidosGlobalStringID call_id_;
	
	EidosValueMask return_mask_;						// a mask specifying the exact return type; the singleton flag is used, the optional flag is not
	const EidosObjectClass *return_class_;				// optional type-check for object returns; used only if the return is an object and this is not nullptr
	
	std::vector<EidosValueMask> arg_masks_;				// the expected types for each argument, as a mask
	std::vector<std::string> arg_names_;				// the argument names as std::strings
	std::vector<EidosGlobalStringID> arg_name_IDs_;		// the argument names as EidosGlobalStringIDs, allowing fast argument list processing
	std::vector<const EidosObjectClass *> arg_classes_;	// the expected object classes for each argument; nullptr unless the argument is object type and specified an element type
	std::vector<EidosValue_SP> arg_defaults_;			// default values for each argument; will be nullptr for required arguments
	
	bool has_optional_args_ = false;					// if true, at least one optional argument has been added
	bool has_ellipsis_ = false;							// if true, the function accepts arbitrary varargs after the specified arguments
	
	
	EidosCallSignature(const EidosCallSignature&) = delete;					// no copying
	EidosCallSignature& operator=(const EidosCallSignature&) = delete;		// no copying
	EidosCallSignature(void) = delete;										// no null construction
	virtual ~EidosCallSignature(void);
	
	EidosCallSignature(const std::string &p_call_name, EidosValueMask p_return_mask);
	EidosCallSignature(const std::string &p_call_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class);
	
	// C++ doesn't have Objective-C's instancetype return, but that's what is needed for all of these...
	// instead, callers will have to cast back to the correct subclass type
	EidosCallSignature *AddArg(EidosValueMask p_arg_mask, const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddArgWithDefault(EidosValueMask p_arg_mask, const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value, bool p_fault_tolerant=false);
	EidosCallSignature *AddEllipsis(void);
	
	// vanilla type-specified arguments
	EidosCallSignature *AddLogical(const std::string &p_argument_name);
	EidosCallSignature *AddInt(const std::string &p_argument_name);
	EidosCallSignature *AddFloat(const std::string &p_argument_name);
	EidosCallSignature *AddIntString(const std::string &p_argument_name);
	EidosCallSignature *AddString(const std::string &p_argument_name);
	EidosCallSignature *AddIntObject(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddObject(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv(const std::string &p_argument_name);
	EidosCallSignature *AddAnyBase(const std::string &p_argument_name);
	EidosCallSignature *AddAny(const std::string &p_argument_name);
	
	// optional arguments
	EidosCallSignature *AddLogical_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddInt_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddFloat_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntString_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddString_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntObject_O(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddObject_O(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddNumeric_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddLogicalEquiv_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddAnyBase_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddAny_O(const std::string &p_argument_name, EidosValue_SP p_default_value);
	
	// singleton arguments (i.e. required to have a size of exactly 1)
	EidosCallSignature *AddLogical_S(const std::string &p_argument_name);
	EidosCallSignature *AddInt_S(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_S(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_S(const std::string &p_argument_name);
	EidosCallSignature *AddString_S(const std::string &p_argument_name);
	EidosCallSignature *AddIntObject_S(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddObject_S(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_S(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_S(const std::string &p_argument_name);
	EidosCallSignature *AddAnyBase_S(const std::string &p_argument_name);
	EidosCallSignature *AddAny_S(const std::string &p_argument_name);
	
	// optional singleton arguments
	EidosCallSignature *AddLogical_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddInt_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddFloat_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntString_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddString_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntObject_OS(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddObject_OS(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddNumeric_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddLogicalEquiv_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddAnyBase_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddAny_OS(const std::string &p_argument_name, EidosValue_SP p_default_value);
	
	// type-specified or NULL
	EidosCallSignature *AddLogical_N(const std::string &p_argument_name);
	EidosCallSignature *AddInt_N(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_N(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_N(const std::string &p_argument_name);
	EidosCallSignature *AddString_N(const std::string &p_argument_name);
	EidosCallSignature *AddIntObject_N(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddObject_N(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_N(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_N(const std::string &p_argument_name);
	
	// optional type-specified or NULL
	EidosCallSignature *AddLogical_ON(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddInt_ON(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddFloat_ON(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntString_ON(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddString_ON(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntObject_ON(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddObject_ON(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddNumeric_ON(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddLogicalEquiv_ON(const std::string &p_argument_name, EidosValue_SP p_default_value);
	
	// singleton type-specified or NULL
	EidosCallSignature *AddLogical_SN(const std::string &p_argument_name);
	EidosCallSignature *AddInt_SN(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_SN(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_SN(const std::string &p_argument_name);
	EidosCallSignature *AddString_SN(const std::string &p_argument_name);
	EidosCallSignature *AddIntObject_SN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddObject_SN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_SN(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_SN(const std::string &p_argument_name);
	
	// optional singleton type-specified or NULL
	EidosCallSignature *AddLogical_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddInt_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddFloat_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntString_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddString_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddIntObject_OSN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddObject_OSN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class, EidosValue_SP p_default_value);
	EidosCallSignature *AddNumeric_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value);
	EidosCallSignature *AddLogicalEquiv_OSN(const std::string &p_argument_name, EidosValue_SP p_default_value);
	
	// check arguments and returns
	void CheckArguments(const EidosValue_SP *const p_arguments, unsigned int p_argument_count) const;
	void CheckReturn(const EidosValue &p_result) const;
	void CheckAggregateReturn(const EidosValue &p_result, size_t p_expected_size) const;
	
	// Get the signature as a string, via operator<<
	std::string SignatureString(void) const;
	
	// virtual functions defined by the subclasses below
	virtual std::string CallType(void) const = 0;				// "function" or "method"; used for error output
	virtual std::string CallPrefix(void) const = 0;				// "", "– ", or "+ "; used for stream output
	virtual std::string CallDelegate(void) const;				// used for stream output
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosCallSignature &p_signature);
bool CompareEidosCallSignatures(const EidosCallSignature *p_i, const EidosCallSignature *p_j);


#pragma mark -
#pragma mark EidosFunctionSignature
#pragma mark -

class EidosFunctionSignature : public EidosCallSignature
{
public:
	// internal function implementations
	EidosInternalFunctionPtr internal_function_ = nullptr;
	
	// delegated function implementations; these are dispatched through the Context object in the interpreter
	std::string delegate_name_;
	
	// user-defined functions; these are dispatched using a sub-interpreter (OWNED pointer)
	EidosScript *body_script_ = nullptr;
	bool user_defined_ = false;
	
	EidosFunctionSignature(const EidosFunctionSignature&) = delete;								// no copying
	EidosFunctionSignature& operator=(const EidosFunctionSignature&) = delete;					// no copying
	EidosFunctionSignature(void) = delete;														// no null construction
	virtual ~EidosFunctionSignature(void);
	
	EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask);
	EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class);
	EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask, const std::string &p_delegate_name);
	EidosFunctionSignature(const std::string &p_function_name, EidosInternalFunctionPtr p_function_ptr, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class, const std::string &p_delegate_name);
	
	virtual std::string CallType(void) const;
	virtual std::string CallPrefix(void) const;
	virtual std::string CallDelegate(void) const;
};

// Function signatures are kept under shared_ptr since user-defined functions make their lifetime complicated
typedef std::shared_ptr<const EidosFunctionSignature> EidosFunctionSignature_SP;
bool CompareEidosFunctionSignature_SPs(EidosFunctionSignature_SP p_i, EidosFunctionSignature_SP p_j);


#pragma mark -
#pragma mark EidosMethodSignature
#pragma mark -

class EidosMethodSignature : public EidosCallSignature
{
public:
	bool is_class_method = false;	// a bit unfortunate, but much faster to check this than to call a virtual function...
	
	EidosMethodSignature(const EidosMethodSignature&) = delete;									// no copying
	EidosMethodSignature& operator=(const EidosMethodSignature&) = delete;						// no copying
	EidosMethodSignature(void) = delete;														// no null construction
	virtual ~EidosMethodSignature(void);
	
	EidosMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, bool p_is_class_method);
	EidosMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class, bool p_is_class_method);
	
	virtual std::string CallType(void) const;
};


#pragma mark -
#pragma mark EidosInstanceMethodSignature
#pragma mark -

class EidosInstanceMethodSignature : public EidosMethodSignature
{
public:
	bool accelerated_imp_;									// if true, the method has a special vectorized implementation
	Eidos_AcceleratedMethodImp accelerated_imper_;			// a pointer to a (static member) function that handles the accelerated imp
	
	EidosInstanceMethodSignature(const EidosInstanceMethodSignature&) = delete;					// no copying
	EidosInstanceMethodSignature& operator=(const EidosInstanceMethodSignature&) = delete;		// no copying
	EidosInstanceMethodSignature(void) = delete;												// no null construction
	virtual ~EidosInstanceMethodSignature(void);
	
	EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask);
	EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class);
	
	virtual std::string CallPrefix(void) const;
	
	// property access acceleration
	EidosInstanceMethodSignature *DeclareAcceleratedImp(Eidos_AcceleratedMethodImp p_imper);
};


#pragma mark -
#pragma mark EidosClassMethodSignature
#pragma mark -

class EidosClassMethodSignature : public EidosMethodSignature
{
public:
	EidosClassMethodSignature(const EidosClassMethodSignature&) = delete;						// no copying
	EidosClassMethodSignature& operator=(const EidosClassMethodSignature&) = delete;			// no copying
	EidosClassMethodSignature(void) = delete;													// no null construction
	virtual ~EidosClassMethodSignature(void);
	
	EidosClassMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask);
	EidosClassMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class);
	
	virtual std::string CallPrefix(void) const;
};


#endif /* defined(__Eidos__eidos_function_signature__) */































































