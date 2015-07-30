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


// EidosFunctionSignature is used to represent the return type and argument types of a function or method, for shared runtime type checking
class EidosFunctionSignature
{
public:
	std::string function_name_;
	EidosFunctionIdentifier function_id_;
	EidosValueMask return_mask_;				// a mask specifying the exact return type; the singleton flag is used, the optional flag is not
	std::vector<EidosValueMask> arg_masks_;		// the expected types for each argument, as a mask
	std::vector<std::string> arg_names_;		// the expected types for each argument, as a mask
	bool is_class_method = false;				// if true, the function is a class method and so will not be multiplexed
	bool is_instance_method = false;			// if true, the function is an instance method (affects operator << only, right now)
	bool has_optional_args_ = false;			// if true, at least one optional argument has been added
	bool has_ellipsis_ = false;					// if true, the function accepts arbitrary varargs after the specified arguments
	
	// ivars related to delegated function implementations
	EidosDelegateFunctionPtr delegate_function_ = nullptr;
	void *delegate_object_ = nullptr;
	std::string delegate_name_;
	
	EidosFunctionSignature(const EidosFunctionSignature&) = delete;					// no copying
	EidosFunctionSignature& operator=(const EidosFunctionSignature&) = delete;		// no copying
	EidosFunctionSignature(void) = delete;										// no null construction
	
	EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask);
	EidosFunctionSignature(const std::string &p_function_name, EidosFunctionIdentifier p_function_id, EidosValueMask p_return_mask, EidosDelegateFunctionPtr p_delegate_function, void *p_delegate_object, const std::string &p_delegate_name);
	
	EidosFunctionSignature *SetClassMethod();
	EidosFunctionSignature *SetInstanceMethod();
	
	EidosFunctionSignature *AddArg(EidosValueMask p_arg_mask, const std::string &p_argument_name);
	EidosFunctionSignature *AddEllipsis();
	
	// vanilla type-specified arguments
	EidosFunctionSignature *AddLogical(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat(const std::string &p_argument_name);
	EidosFunctionSignature *AddString(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv(const std::string &p_argument_name);
	EidosFunctionSignature *AddAnyBase(const std::string &p_argument_name);
	EidosFunctionSignature *AddAny(const std::string &p_argument_name);
	
	// optional arguments
	EidosFunctionSignature *AddLogical_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddString_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddAnyBase_O(const std::string &p_argument_name);
	EidosFunctionSignature *AddAny_O(const std::string &p_argument_name);
	
	// singleton arguments (i.e. required to have a size of exactly 1)
	EidosFunctionSignature *AddLogical_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddString_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddAnyBase_S(const std::string &p_argument_name);
	EidosFunctionSignature *AddAny_S(const std::string &p_argument_name);
	
	// optional singleton arguments
	EidosFunctionSignature *AddLogical_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddString_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddAnyBase_OS(const std::string &p_argument_name);
	EidosFunctionSignature *AddAny_OS(const std::string &p_argument_name);
	
	// type-specified or NULL
	EidosFunctionSignature *AddLogical_N(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt_N(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat_N(const std::string &p_argument_name);
	EidosFunctionSignature *AddString_N(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject_N(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric_N(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv_N(const std::string &p_argument_name);
	
	// optional type-specified or NULL
	EidosFunctionSignature *AddLogical_ON(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt_ON(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat_ON(const std::string &p_argument_name);
	EidosFunctionSignature *AddString_ON(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject_ON(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric_ON(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv_ON(const std::string &p_argument_name);
	
	// singleton type-specified or NULL
	EidosFunctionSignature *AddLogical_SN(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt_SN(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat_SN(const std::string &p_argument_name);
	EidosFunctionSignature *AddString_SN(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject_SN(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric_SN(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv_SN(const std::string &p_argument_name);
	
	// optional singleton type-specified or NULL
	EidosFunctionSignature *AddLogical_OSN(const std::string &p_argument_name);
	EidosFunctionSignature *AddInt_OSN(const std::string &p_argument_name);
	EidosFunctionSignature *AddFloat_OSN(const std::string &p_argument_name);
	EidosFunctionSignature *AddString_OSN(const std::string &p_argument_name);
	EidosFunctionSignature *AddObject_OSN(const std::string &p_argument_name);
	EidosFunctionSignature *AddNumeric_OSN(const std::string &p_argument_name);
	EidosFunctionSignature *AddLogicalEquiv_OSN(const std::string &p_argument_name);
	
	// check an argument list; p_call_type should be "function" or "method", for error output only
	void CheckArguments(const std::string &p_call_type, EidosValue *const *const p_arguments, int p_argument_count) const;
	
	// check a return value; p_call_type should be "function" or "method", for error output only
	void CheckReturn(const std::string &p_call_type, EidosValue *p_result) const;
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosFunctionSignature &p_signature);
bool CompareEidosFunctionSignatures(const EidosFunctionSignature *i, const EidosFunctionSignature *j);


#endif /* defined(__Eidos__eidos_function_signature__) */































































