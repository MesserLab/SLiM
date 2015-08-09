//
//  eidos_function_signature.h
//  Eidos
//
//  Created by Ben Haller on 5/16/15.
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


#ifndef __Eidos__eidos_function_signature__
#define __Eidos__eidos_function_signature__

#include "eidos_interpreter.h"
#include "eidos_value.h"

#include <iostream>


// EidosCallSignature is used to represent the return type and argument types of a function or method, for shared runtime type checking.
// This is an abstract base class; clients should use one of the subclasses below.
class EidosCallSignature
{
public:
	std::string function_name_;
	EidosFunctionIdentifier function_id_;
	
	EidosValueMask return_mask_;						// a mask specifying the exact return type; the singleton flag is used, the optional flag is not
	const EidosObjectClass *return_class_;				// optional type-check for object returns; used only if the return is an object and this is not nullptr
	
	std::vector<EidosValueMask> arg_masks_;				// the expected types for each argument, as a mask
	std::vector<std::string> arg_names_;				// the expected types for each argument, as a mask
	std::vector<const EidosObjectClass *> arg_classes_;	// the expected object classes for each argument; nullptr unless the argument is object type and specified an element type
	
	bool has_optional_args_ = false;					// if true, at least one optional argument has been added
	bool has_ellipsis_ = false;							// if true, the function accepts arbitrary varargs after the specified arguments
	
	
	EidosCallSignature(const EidosCallSignature&) = delete;					// no copying
	EidosCallSignature& operator=(const EidosCallSignature&) = delete;		// no copying
	EidosCallSignature(void) = delete;										// no null construction
	virtual ~EidosCallSignature(void);
	
	EidosCallSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask);
	EidosCallSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class);
	
	// C++ doesn't have Objective-C's instancetype return, but that's what is needed for all of these...
	// instead, callers will have to cast back to the correct subclass type
	EidosCallSignature *AddArg(EidosValueMask p_arg_mask, const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddEllipsis();
	
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
	EidosCallSignature *AddLogical_O(const std::string &p_argument_name);
	EidosCallSignature *AddInt_O(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_O(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_O(const std::string &p_argument_name);
	EidosCallSignature *AddString_O(const std::string &p_argument_name);
	EidosCallSignature *AddIntObject_O(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddObject_O(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_O(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_O(const std::string &p_argument_name);
	EidosCallSignature *AddAnyBase_O(const std::string &p_argument_name);
	EidosCallSignature *AddAny_O(const std::string &p_argument_name);
	
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
	EidosCallSignature *AddLogical_OS(const std::string &p_argument_name);
	EidosCallSignature *AddInt_OS(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_OS(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_OS(const std::string &p_argument_name);
	EidosCallSignature *AddString_OS(const std::string &p_argument_name);
	EidosCallSignature *AddIntObject_OS(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddObject_OS(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_OS(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_OS(const std::string &p_argument_name);
	EidosCallSignature *AddAnyBase_OS(const std::string &p_argument_name);
	EidosCallSignature *AddAny_OS(const std::string &p_argument_name);
	
	// type-specified or NULL
	EidosCallSignature *AddLogical_N(const std::string &p_argument_name);
	EidosCallSignature *AddInt_N(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_N(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_N(const std::string &p_argument_name);
	EidosCallSignature *AddString_N(const std::string &p_argument_name);
	EidosCallSignature *AddObject_N(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_N(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_N(const std::string &p_argument_name);
	
	// optional type-specified or NULL
	EidosCallSignature *AddLogical_ON(const std::string &p_argument_name);
	EidosCallSignature *AddInt_ON(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_ON(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_ON(const std::string &p_argument_name);
	EidosCallSignature *AddString_ON(const std::string &p_argument_name);
	EidosCallSignature *AddObject_ON(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_ON(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_ON(const std::string &p_argument_name);
	
	// singleton type-specified or NULL
	EidosCallSignature *AddLogical_SN(const std::string &p_argument_name);
	EidosCallSignature *AddInt_SN(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_SN(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_SN(const std::string &p_argument_name);
	EidosCallSignature *AddString_SN(const std::string &p_argument_name);
	EidosCallSignature *AddObject_SN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_SN(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_SN(const std::string &p_argument_name);
	
	// optional singleton type-specified or NULL
	EidosCallSignature *AddLogical_OSN(const std::string &p_argument_name);
	EidosCallSignature *AddInt_OSN(const std::string &p_argument_name);
	EidosCallSignature *AddFloat_OSN(const std::string &p_argument_name);
	EidosCallSignature *AddIntString_OSN(const std::string &p_argument_name);
	EidosCallSignature *AddString_OSN(const std::string &p_argument_name);
	EidosCallSignature *AddObject_OSN(const std::string &p_argument_name, const EidosObjectClass *p_argument_class);
	EidosCallSignature *AddNumeric_OSN(const std::string &p_argument_name);
	EidosCallSignature *AddLogicalEquiv_OSN(const std::string &p_argument_name);
	
	// check arguments and returns
	void CheckArguments(EidosValue *const *const p_arguments, int p_argument_count) const;
	void CheckReturn(EidosValue *p_result) const;
	
	// virtual functions defined by the subclasses below
	virtual std::string CallType(void) const = 0;				// "function" or "method"; used for error output
	virtual std::string CallPrefix(void) const = 0;				// "", "- ", or "+ "; used for stream output
	virtual std::string CallDelegate(void) const;				// used for stream output
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosCallSignature &p_signature);
bool CompareEidosCallSignatures(const EidosCallSignature *i, const EidosCallSignature *j);


class EidosFunctionSignature : public EidosCallSignature
{
public:
	// ivars related to delegated function implementations
	EidosDelegateFunctionPtr delegate_function_ = nullptr;
	void *delegate_object_ = nullptr;
	std::string delegate_name_;
	
	EidosFunctionSignature(const EidosFunctionSignature&) = delete;								// no copying
	EidosFunctionSignature& operator=(const EidosFunctionSignature&) = delete;					// no copying
	EidosFunctionSignature(void) = delete;														// no null construction
	virtual ~EidosFunctionSignature(void);
	
	EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask);
	EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class);
	EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, EidosDelegateFunctionPtr p_delegate_function, void *p_delegate_object, const std::string &p_delegate_name);
	EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class, EidosDelegateFunctionPtr p_delegate_function, void *p_delegate_object, const std::string &p_delegate_name);
	
	virtual std::string CallType(void) const;
	virtual std::string CallPrefix(void) const;
	virtual std::string CallDelegate(void) const;
};

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

class EidosInstanceMethodSignature : public EidosMethodSignature
{
public:
	EidosInstanceMethodSignature(const EidosInstanceMethodSignature&) = delete;					// no copying
	EidosInstanceMethodSignature& operator=(const EidosInstanceMethodSignature&) = delete;		// no copying
	EidosInstanceMethodSignature(void) = delete;												// no null construction
	virtual ~EidosInstanceMethodSignature(void);
	
	EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask);
	EidosInstanceMethodSignature(const std::string &p_function_name, EidosValueMask p_return_mask, const EidosObjectClass *p_return_class);
	
	virtual std::string CallPrefix(void) const;
};

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































































