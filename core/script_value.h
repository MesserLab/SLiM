//
//  script_value.h
//  SLiM
//
//  Created by Ben Haller on 4/7/15.
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

/*
 
 The class ScriptValue represents any variable value in a ScriptInterpreter context.  ScriptValue itself is an abstract base class.
 Subclasses define types for NULL, logical, string, integer, and float types.  ScriptValue types are generally vectors (although
 null is an exception); there are no scalar types in SLiMScript.
 
 */

#ifndef __SLiM__script_value__
#define __SLiM__script_value__

#include <iostream>
#include <vector>
#include <string>

#include "script_symbols.h"


class ScriptValue;


// all of these types are vectors of the stated type; all objects in SLiMScript are vectors
// the order of these types is in type-promotion order from lowest to highest
enum class ScriptValueType {
	kValueNULL = 0,		// special NULL value
	kValueLogical,		// logicals (bools)
	kValueInt,			// (64-bit) integers
	kValueFloat,		// (double-precision) floats
	kValueString,		// strings
	
	kValueProxy			// proxy objects: these represent built-in struct-type objects with member components
};

enum class ScriptValueMask : uint32_t {
	kMaskNone =				0x00000000,
	kMaskNULL =				0x00000001,
	kMaskLogical =			0x00000002,
	kMaskInt =				0x00000004,
	kMaskFloat =			0x00000008,
	kMaskString =			0x00000010,
	kMaskProxy =			0x00000020,
	
	kMaskNumeric =			(kMaskInt | kMaskFloat),														// integer or float
	kMaskLogicalEquiv =		(kMaskLogical | kMaskInt | kMaskFloat),											// logical, integer, or float: boolean-compatible
	kMaskAnyBase =			(kMaskNULL | kMaskLogical | kMaskString | kMaskInt | kMaskFloat),				// any type except proxy
	kMaskAny =				(kMaskNULL | kMaskLogical | kMaskString | kMaskInt | kMaskFloat | kMaskProxy)	// any type including proxy
};


//
//	Associated functions
//

int CompareScriptValues(const ScriptValue *p_value1, int p_index1, const ScriptValue *p_value2, int p_index2);

std::string StringForScriptValueType(const ScriptValueType p_type);
std::ostream &operator<<(std::ostream &p_outstream, const ScriptValueType p_type);


// A class representing a value resulting from script evaluation.  SLiMScript is quite dynamically typed;
// problems cause runtime exceptions.  ScriptValue is the abstract base class for all value classes.
class ScriptValue : public SymbolHost
{
	//	This class has its assignment operator disabled, to prevent accidental copying.
private:
	
	bool in_symbol_table_ = false;							// if true, the value should not be deleted, as it is owned by the symbol table
	
protected:
	
	bool invisible_ = false;								// as in R; if true, the value will not normally be printed to the console
	
public:
	
	ScriptValue(const ScriptValue &p_original);				// can copy-construct (but see the definition; NOT default)
	ScriptValue& operator=(const ScriptValue&) = delete;	// no copying
	
	ScriptValue(void);										// null constructor for base class
	virtual ~ScriptValue(void) = 0;							// virtual destructor
	
	// basic methods
	virtual ScriptValueType Type(void) const = 0;			// the type contained by the vector
	virtual int Count(void) const = 0;						// the number of values in the vector
	virtual void Print(std::ostream &p_ostream) const = 0;	// standard printing
	
	bool Invisible(void) const;								// getter only; invisible objects must be made through construction or InvisibleCopy()
	
	bool InSymbolTable(void) const;
	ScriptValue *SetInSymbolTable(bool p_in_symbol_table);
	
	// basic subscript access from SymbolHost; still abstract here since we want to force subclasses to define this
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const = 0;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value) = 0;
	
	// basic member access from SymbolHost; defined here to raise with an error message
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name) const;
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	// fetching individual values; these convert type if necessary, and (base class behavior) raise if impossible
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	// methods to allow type-agnostic manipulation of ScriptValues
	virtual ScriptValue *CopyValues(void) const = 0;			// a deep copy of the receiver with in_symbol_table_ == invisible_ == false
	virtual ScriptValue *NewMatchingType(void) const = 0;		// a new ScriptValue instance of the same type as the receiver
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value) = 0;	// copy a value from another object of the same type
};

std::ostream &operator<<(std::ostream &p_outstream, const ScriptValue &p_value);

class ScriptValue_NULL : public ScriptValue
{
public:
	ScriptValue_NULL(const ScriptValue_NULL &p_original) = default;	// can copy-construct
	ScriptValue_NULL& operator=(const ScriptValue_NULL&) = delete;	// no copying
	
	ScriptValue_NULL(void);
	virtual ~ScriptValue_NULL(void);
	
	static ScriptValue_NULL *ScriptValue_NULL_Invisible(void);		// factory method for invisible null objects, since that is a common need
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);

	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
};

class ScriptValue_Logical : public ScriptValue
{
private:
	std::vector<bool> values_;
	
public:
	ScriptValue_Logical(const ScriptValue_Logical &p_original) = default;		// can copy-construct
	ScriptValue_Logical& operator=(const ScriptValue_Logical&) = delete;	// no copying
	
	ScriptValue_Logical(void);
	explicit ScriptValue_Logical(bool p_bool1);
	ScriptValue_Logical(bool p_bool1, bool p_bool2);
	ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3);
	ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4);
	ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5);
	ScriptValue_Logical(bool p_bool1, bool p_bool2, bool p_bool3, bool p_bool4, bool p_bool5, bool p_bool6);
	virtual ~ScriptValue_Logical(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	void PushLogical(bool p_logical);
	virtual void SetLogicalAtIndex(const int p_idx, bool p_logical);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
};

class ScriptValue_String : public ScriptValue
{
private:
	std::vector<std::string> values_;
	
public:
	ScriptValue_String(const ScriptValue_String &p_original) = default;	// can copy-construct
	ScriptValue_String& operator=(const ScriptValue_String&) = delete;	// no copying
	
	ScriptValue_String(void);
	explicit ScriptValue_String(std::string p_string1);
	ScriptValue_String(std::string p_string1, std::string p_string2);
	ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3);
	ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3, std::string p_string4);
	ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3, std::string p_string4, std::string p_string5);
	ScriptValue_String(std::string p_string1, std::string p_string2, std::string p_string3, std::string p_string4, std::string p_string5, std::string p_string6);
	virtual ~ScriptValue_String(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	void PushString(std::string p_string);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
};

class ScriptValue_Int : public ScriptValue
{
private:
	std::vector<int64_t> values_;
	
public:
	ScriptValue_Int(const ScriptValue_Int &p_original) = default;		// can copy-construct
	ScriptValue_Int& operator=(const ScriptValue_Int&) = delete;	// no copying
	
	ScriptValue_Int(void);
	explicit ScriptValue_Int(int64_t p_int1);
	ScriptValue_Int(int64_t p_int1, int64_t p_int2);
	ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3);
	ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4);
	ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5);
	ScriptValue_Int(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5, int64_t p_int6);
	virtual ~ScriptValue_Int(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	void PushInt(int64_t p_int);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
};

class ScriptValue_Float : public ScriptValue
{
private:
	std::vector<double> values_;
	
public:
	ScriptValue_Float(const ScriptValue_Float &p_original) = default;		// can copy-construct
	ScriptValue_Float& operator=(const ScriptValue_Float&) = delete;	// no copying
	
	ScriptValue_Float(void);
	explicit ScriptValue_Float(double p_float1);
	ScriptValue_Float(double p_float1, double p_float2);
	ScriptValue_Float(double p_float1, double p_float2, double p_float3);
	ScriptValue_Float(double p_float1, double p_float2, double p_float3, double p_float4);
	ScriptValue_Float(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5);
	ScriptValue_Float(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5, double p_float6);
	virtual ~ScriptValue_Float(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	void PushFloat(double p_float);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
};

class ScriptValue_Proxy : public ScriptValue
{
public:
	ScriptValue_Proxy(const ScriptValue_Proxy &p_original) = delete;		// can copy-construct
	ScriptValue_Proxy& operator=(const ScriptValue_Proxy&) = delete;	// no copying
	
	ScriptValue_Proxy(void) = default;
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual std::string ProxyType(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
};

class ScriptValue_PathProxy : public ScriptValue_Proxy
{
private:
	std::string base_path_;
	
public:
	ScriptValue_PathProxy(const ScriptValue_PathProxy &p_original) = delete;		// can copy-construct
	ScriptValue_PathProxy& operator=(const ScriptValue_PathProxy&) = delete;	// no copying
	
	ScriptValue_PathProxy(void);
	explicit ScriptValue_PathProxy(std::string p_base_path);
	
	std::string ProxyType(void) const;
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name) const;
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
};


#endif /* defined(__SLiM__script_value__) */




































































