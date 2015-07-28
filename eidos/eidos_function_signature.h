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
	EidosValueMask return_mask_;					// a mask specifying the exact return type; the singleton flag is used, the optional flag is not
	std::vector<EidosValueMask> arg_masks_;		// the expected types for each argument, as a mask
	bool is_class_method = false;					// if true, the function is a class method and so will not be multiplexed
	bool is_instance_method = false;				// if true, the function is an instance method (affects operator << only, right now)
	bool has_optional_args_ = false;				// if true, at least one optional argument has been added
	bool has_ellipsis_ = false;						// if true, the function accepts arbitrary varargs after the specified arguments
	
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
	
	EidosFunctionSignature *AddArg(EidosValueMask p_arg_mask);
	EidosFunctionSignature *AddEllipsis();
	
	// vanilla type-specified arguments
	EidosFunctionSignature *AddLogical();
	EidosFunctionSignature *AddInt();
	EidosFunctionSignature *AddFloat();
	EidosFunctionSignature *AddString();
	EidosFunctionSignature *AddObject();
	EidosFunctionSignature *AddNumeric();
	EidosFunctionSignature *AddLogicalEquiv();
	EidosFunctionSignature *AddAnyBase();
	EidosFunctionSignature *AddAny();
	
	// optional arguments
	EidosFunctionSignature *AddLogical_O();
	EidosFunctionSignature *AddInt_O();
	EidosFunctionSignature *AddFloat_O();
	EidosFunctionSignature *AddString_O();
	EidosFunctionSignature *AddObject_O();
	EidosFunctionSignature *AddNumeric_O();
	EidosFunctionSignature *AddLogicalEquiv_O();
	EidosFunctionSignature *AddAnyBase_O();
	EidosFunctionSignature *AddAny_O();
	
	// singleton arguments (i.e. required to have a size of exactly 1)
	EidosFunctionSignature *AddLogical_S();
	EidosFunctionSignature *AddInt_S();
	EidosFunctionSignature *AddFloat_S();
	EidosFunctionSignature *AddString_S();
	EidosFunctionSignature *AddObject_S();
	EidosFunctionSignature *AddNumeric_S();
	EidosFunctionSignature *AddLogicalEquiv_S();
	EidosFunctionSignature *AddAnyBase_S();
	EidosFunctionSignature *AddAny_S();
	
	// optional singleton arguments
	EidosFunctionSignature *AddLogical_OS();
	EidosFunctionSignature *AddInt_OS();
	EidosFunctionSignature *AddFloat_OS();
	EidosFunctionSignature *AddString_OS();
	EidosFunctionSignature *AddObject_OS();
	EidosFunctionSignature *AddNumeric_OS();
	EidosFunctionSignature *AddLogicalEquiv_OS();
	EidosFunctionSignature *AddAnyBase_OS();
	EidosFunctionSignature *AddAny_OS();
	
	// type-specified or NULL
	EidosFunctionSignature *AddLogical_N();
	EidosFunctionSignature *AddInt_N();
	EidosFunctionSignature *AddFloat_N();
	EidosFunctionSignature *AddString_N();
	EidosFunctionSignature *AddObject_N();
	EidosFunctionSignature *AddNumeric_N();
	EidosFunctionSignature *AddLogicalEquiv_N();
	
	// optional type-specified or NULL
	EidosFunctionSignature *AddLogical_ON();
	EidosFunctionSignature *AddInt_ON();
	EidosFunctionSignature *AddFloat_ON();
	EidosFunctionSignature *AddString_ON();
	EidosFunctionSignature *AddObject_ON();
	EidosFunctionSignature *AddNumeric_ON();
	EidosFunctionSignature *AddLogicalEquiv_ON();
	
	// singleton type-specified or NULL
	EidosFunctionSignature *AddLogical_SN();
	EidosFunctionSignature *AddInt_SN();
	EidosFunctionSignature *AddFloat_SN();
	EidosFunctionSignature *AddString_SN();
	EidosFunctionSignature *AddObject_SN();
	EidosFunctionSignature *AddNumeric_SN();
	EidosFunctionSignature *AddLogicalEquiv_SN();
	
	// optional singleton type-specified or NULL
	EidosFunctionSignature *AddLogical_OSN();
	EidosFunctionSignature *AddInt_OSN();
	EidosFunctionSignature *AddFloat_OSN();
	EidosFunctionSignature *AddString_OSN();
	EidosFunctionSignature *AddObject_OSN();
	EidosFunctionSignature *AddNumeric_OSN();
	EidosFunctionSignature *AddLogicalEquiv_OSN();
	
	// check an argument list; p_call_type should be "function" or "method", for error output only
	void CheckArguments(const std::string &p_call_type, EidosValue *const *const p_arguments, int p_argument_count) const;
	
	// check a return value; p_call_type should be "function" or "method", for error output only
	void CheckReturn(const std::string &p_call_type, EidosValue *p_result) const;
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosFunctionSignature &p_signature);
bool CompareEidosFunctionSignatures(const EidosFunctionSignature *i, const EidosFunctionSignature *j);


#endif /* defined(__Eidos__eidos_function_signature__) */































































