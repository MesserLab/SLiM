//
//  script_functionsignature.h
//  SLiM
//
//  Created by Ben Haller on 5/16/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#ifndef __SLiM__script_functionsignature__
#define __SLiM__script_functionsignature__

#include "script_interpreter.h"
#include "script_value.h"

#include <iostream>


// FunctionSignature is used to represent the return type and argument types of a function or method, for shared runtime type checking
class FunctionSignature
{
public:
	std::string function_name_;
	FunctionIdentifier function_id_;
	ScriptValueMask return_mask_;					// a mask specifying the exact return type; the singleton flag is used, the optional flag is not
	std::vector<ScriptValueMask> arg_masks_;		// the expected types for each argument, as a mask
	bool is_class_method = false;					// if true, the function is a class method and so will not be multiplexed
	bool is_instance_method = false;				// if true, the function is an instance method (affects operator << only, right now)
	bool has_optional_args_ = false;				// if true, at least one optional argument has been added
	bool has_ellipsis_ = false;						// if true, the function accepts arbitrary varargs after the specified arguments
	
	// ivars related to delegated function implementations
	SLiMDelegateFunctionPtr delegate_function_ = nullptr;
	void *delegate_object_ = nullptr;
	std::string delegate_name_;
	
	FunctionSignature(const FunctionSignature&) = delete;					// no copying
	FunctionSignature& operator=(const FunctionSignature&) = delete;		// no copying
	FunctionSignature(void) = delete;										// no null construction
	
	FunctionSignature(std::string p_function_name, FunctionIdentifier p_function_id, ScriptValueMask p_return_mask);
	FunctionSignature(std::string p_function_name, FunctionIdentifier p_function_id, ScriptValueMask p_return_mask, SLiMDelegateFunctionPtr p_delegate_function, void *p_delegate_object, std::string p_delegate_name);
	
	FunctionSignature *SetClassMethod();
	FunctionSignature *SetInstanceMethod();
	
	FunctionSignature *AddArg(ScriptValueMask p_arg_mask);
	FunctionSignature *AddEllipsis();
	
	// vanilla type-specified arguments
	FunctionSignature *AddLogical();
	FunctionSignature *AddInt();
	FunctionSignature *AddFloat();
	FunctionSignature *AddString();
	FunctionSignature *AddObject();
	FunctionSignature *AddNumeric();
	FunctionSignature *AddLogicalEquiv();
	FunctionSignature *AddAnyBase();
	FunctionSignature *AddAny();
	
	// optional arguments
	FunctionSignature *AddLogical_O();
	FunctionSignature *AddInt_O();
	FunctionSignature *AddFloat_O();
	FunctionSignature *AddString_O();
	FunctionSignature *AddObject_O();
	FunctionSignature *AddNumeric_O();
	FunctionSignature *AddLogicalEquiv_O();
	FunctionSignature *AddAnyBase_O();
	FunctionSignature *AddAny_O();
	
	// singleton arguments (i.e. required to have a size of exactly 1)
	FunctionSignature *AddLogical_S();
	FunctionSignature *AddInt_S();
	FunctionSignature *AddFloat_S();
	FunctionSignature *AddString_S();
	FunctionSignature *AddObject_S();
	FunctionSignature *AddNumeric_S();
	FunctionSignature *AddLogicalEquiv_S();
	FunctionSignature *AddAnyBase_S();
	FunctionSignature *AddAny_S();
	
	// optional singleton arguments
	FunctionSignature *AddLogical_OS();
	FunctionSignature *AddInt_OS();
	FunctionSignature *AddFloat_OS();
	FunctionSignature *AddString_OS();
	FunctionSignature *AddObject_OS();
	FunctionSignature *AddNumeric_OS();
	FunctionSignature *AddLogicalEquiv_OS();
	FunctionSignature *AddAnyBase_OS();
	FunctionSignature *AddAny_OS();
	
	// type-specified or NULL
	FunctionSignature *AddLogical_N();
	FunctionSignature *AddInt_N();
	FunctionSignature *AddFloat_N();
	FunctionSignature *AddString_N();
	FunctionSignature *AddObject_N();
	FunctionSignature *AddNumeric_N();
	FunctionSignature *AddLogicalEquiv_N();
	
	// optional type-specified or NULL
	FunctionSignature *AddLogical_ON();
	FunctionSignature *AddInt_ON();
	FunctionSignature *AddFloat_ON();
	FunctionSignature *AddString_ON();
	FunctionSignature *AddObject_ON();
	FunctionSignature *AddNumeric_ON();
	FunctionSignature *AddLogicalEquiv_ON();
	
	// singleton type-specified or NULL
	FunctionSignature *AddLogical_SN();
	FunctionSignature *AddInt_SN();
	FunctionSignature *AddFloat_SN();
	FunctionSignature *AddString_SN();
	FunctionSignature *AddObject_SN();
	FunctionSignature *AddNumeric_SN();
	FunctionSignature *AddLogicalEquiv_SN();
	
	// optional singleton type-specified or NULL
	FunctionSignature *AddLogical_OSN();
	FunctionSignature *AddInt_OSN();
	FunctionSignature *AddFloat_OSN();
	FunctionSignature *AddString_OSN();
	FunctionSignature *AddObject_OSN();
	FunctionSignature *AddNumeric_OSN();
	FunctionSignature *AddLogicalEquiv_OSN();
	
	// check an argument list; p_call_type should be "function" or "method", for error output only
	void CheckArguments(std::string const &p_call_type, std::vector<ScriptValue*> const &p_arguments) const;
	
	// check a return value; p_call_type should be "function" or "method", for error output only
	void CheckReturn(std::string const &p_call_type, ScriptValue *p_result) const;
};

std::ostream &operator<<(std::ostream &p_outstream, const FunctionSignature &p_signature);
bool CompareFunctionSignatures(const FunctionSignature *i, const FunctionSignature *j);


#endif /* defined(__SLiM__script_functionsignature__) */































































