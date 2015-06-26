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
class ScriptValue_NULL;
class ScriptValue_Logical;

struct FunctionSignature;
class ScriptInterpreter;

class ScriptObjectElement;	// the value type for ScriptValue_Object; defined at the bottom of this file


extern ScriptValue_NULL *gStaticScriptValueNULL;
extern ScriptValue_NULL *gStaticScriptValueNULLInvisible;

extern ScriptValue_Logical *gStaticScriptValue_LogicalT;
extern ScriptValue_Logical *gStaticScriptValue_LogicalF;


// ScriptValueType is an enum of the possible types for ScriptValue objects.  Note that all of these types are vectors of the stated
// type; all objects in SLiMScript are vectors.  The order of these types is in type-promotion order, from lowest to highest, except
// that NULL never gets promoted to any other type, and nothing ever gets promoted to object type.
enum class ScriptValueType
{
	kValueNULL = 0,		// special NULL type; this cannot be mixed with other types or promoted to other types
	
	kValueLogical,		// logicals (bools)
	kValueInt,			// (64-bit) integers
	kValueFloat,		// (double-precision) floats
	kValueString,		// strings
	
	kValueObject		// a vector of ScriptObjectElement objects: these represent built-in objects with member variables and methods
};

std::string StringForScriptValueType(const ScriptValueType p_type);
std::ostream &operator<<(std::ostream &p_outstream, const ScriptValueType p_type);

int CompareScriptValues(const ScriptValue *p_value1, int p_index1, const ScriptValue *p_value2, int p_index2);


// ScriptValueMask is a uint32_t used as a bit mask to identify permitted types for ScriptValue objects (arguments, returns)
typedef uint32_t ScriptValueMask;

const ScriptValueMask kScriptValueMaskNone =			0x00000000;
const ScriptValueMask kScriptValueMaskNULL =			0x00000001;
const ScriptValueMask kScriptValueMaskLogical =			0x00000002;
const ScriptValueMask kScriptValueMaskInt =				0x00000004;
const ScriptValueMask kScriptValueMaskFloat =			0x00000008;
const ScriptValueMask kScriptValueMaskString =			0x00000010;
const ScriptValueMask kScriptValueMaskObject =			0x00000020;
	
const ScriptValueMask kScriptValueMaskOptional =		0x80000000;
const ScriptValueMask kScriptValueMaskSingleton =		0x40000000;
const ScriptValueMask kScriptValueMaskOptSingleton =	(kScriptValueMaskOptional | kScriptValueMaskSingleton);
const ScriptValueMask kScriptValueMaskFlagStrip =		0x3FFFFFFF;
	
const ScriptValueMask kScriptValueMaskNumeric =			(kScriptValueMaskInt | kScriptValueMaskFloat);										// integer or float
const ScriptValueMask kScriptValueMaskLogicalEquiv =	(kScriptValueMaskLogical | kScriptValueMaskInt | kScriptValueMaskFloat);			// logical, integer, or float
const ScriptValueMask kScriptValueMaskAnyBase =			(kScriptValueMaskNULL | kScriptValueMaskLogicalEquiv | kScriptValueMaskString);		// any type except object
const ScriptValueMask kScriptValueMaskAny =				(kScriptValueMaskAnyBase | kScriptValueMaskObject);									// any type including object

std::string StringForScriptValueMask(const ScriptValueMask p_mask);
//std::ostream &operator<<(std::ostream &p_outstream, const ScriptValueMask p_mask);	// can't do this since ScriptValueMask is just uint32_t


//	*********************************************************************************************************
//
//	A class representing a value resulting from script evaluation.  SLiMScript is quite dynamically typed;
//	problems cause runtime exceptions.  ScriptValue is the abstract base class for all value classes.
//

class ScriptValue
{
	//	This class has its assignment operator disabled, to prevent accidental copying.
private:
	
	bool in_symbol_table_ = false;							// if true, the value should not be deleted, as it is owned by the symbol table
	bool externally_owned_ = false;							// if true, even the symbol table should not delete this ScriptValue; it is owned or statically allocated
	
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
	
	// getter only; invisible objects must be made through construction or InvisibleCopy()
	inline bool Invisible(void) const							{ return invisible_; }
	
	// memory management flags; see the comment in script_symbols.h
	inline bool IsTemporary(void) const							{ return !(in_symbol_table_ || externally_owned_); };
	inline bool InSymbolTable(void) const						{ return in_symbol_table_; };
	inline bool ExternallyOwned(void) const						{ return externally_owned_; };
	inline ScriptValue *SetInSymbolTable(bool p_in_table)		{ in_symbol_table_ = p_in_table; return this; };
	inline ScriptValue *SetExternallyOwned()					{ externally_owned_ = true; in_symbol_table_ = true; return this; };
	
	// basic subscript access; abstract here since we want to force subclasses to define this
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const = 0;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value) = 0;
	
	// fetching individual values; these convert type if necessary, and (base class behavior) raise if impossible
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	virtual ScriptObjectElement *ElementAtIndex(int p_idx) const;
	
	// methods to allow type-agnostic manipulation of ScriptValues
	virtual ScriptValue *CopyValues(void) const = 0;			// a deep copy of the receiver with in_symbol_table_ == invisible_ == false
	virtual ScriptValue *NewMatchingType(void) const = 0;		// a new ScriptValue instance of the same type as the receiver
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value) = 0;	// copy a value from another object of the same type
	virtual void Sort(bool p_ascending) = 0;
};

std::ostream &operator<<(std::ostream &p_outstream, const ScriptValue &p_value);


//	*********************************************************************************************************
//
//	ScriptValue_NULL and ScriptValue_NULL_const represent NULL values in SLiMscript.  ScriptValue_NULL_const
//	is used for static global ScriptValue_NULL instances; we have two, one invisible, one not.
//

class ScriptValue_NULL : public ScriptValue
{
public:
	ScriptValue_NULL(const ScriptValue_NULL &p_original) = default;	// can copy-construct
	ScriptValue_NULL& operator=(const ScriptValue_NULL&) = delete;	// no copying
	
	ScriptValue_NULL(void);
	virtual ~ScriptValue_NULL(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);

	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};

class ScriptValue_NULL_const : public ScriptValue_NULL
{
private:
	ScriptValue_NULL_const(void) = default;
	
public:
	ScriptValue_NULL_const(const ScriptValue_NULL_const &p_original) = delete;	// no copy-construct
	ScriptValue_NULL_const& operator=(const ScriptValue_NULL_const&) = delete;	// no copying
	virtual ~ScriptValue_NULL_const(void);										// destructor calls slim_terminate()
	
	static ScriptValue_NULL *Static_ScriptValue_NULL(void);
	static ScriptValue_NULL *Static_ScriptValue_NULL_Invisible(void);
};


//	*********************************************************************************************************
//
//	ScriptValue_Logical and ScriptValue_Logical_const represent logical (bool) values in SLiMscript.
//	ScriptValue_Logical_const is used for static global ScriptValue_Logical instances; we have two, T and F.
//	Because there are only two values, T and F, we don't need a singleton class, we just use those globals.
//

class ScriptValue_Logical : public ScriptValue
{
private:
	std::vector<bool> values_;
	
public:
	ScriptValue_Logical(const ScriptValue_Logical &p_original) = default;		// can copy-construct
	ScriptValue_Logical& operator=(const ScriptValue_Logical&) = delete;	// no copying
	
	ScriptValue_Logical(void);
	explicit ScriptValue_Logical(std::vector<bool> &p_boolvec);
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
	
	const std::vector<bool> &LogicalVector(void) const;
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	virtual void PushLogical(bool p_logical);
	virtual void SetLogicalAtIndex(const int p_idx, bool p_logical);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};

class ScriptValue_Logical_const : public ScriptValue_Logical
{
private:
	ScriptValue_Logical_const(void) = default;
	
public:
	ScriptValue_Logical_const(const ScriptValue_Logical_const &p_original) = delete;	// no copy-construct
	ScriptValue_Logical_const& operator=(const ScriptValue_Logical_const&) = delete;	// no copying
	explicit ScriptValue_Logical_const(bool p_bool1);
	virtual ~ScriptValue_Logical_const(void);											// destructor calls slim_terminate()
	
	static ScriptValue_Logical *Static_ScriptValue_Logical_T(void);
	static ScriptValue_Logical *Static_ScriptValue_Logical_F(void);
	
	// prohibited actions
	virtual void PushLogical(bool p_logical);
	virtual void SetLogicalAtIndex(const int p_idx, bool p_logical);
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	ScriptValue_String represents string values in SLiMscript.  At the present time it doesn't seem worth
//	having a singleton class, since working with strings is not that common in SLiMscript and it unlikely
//	to happen in hotspot areas like callbacks.
//

class ScriptValue_String : public ScriptValue
{
private:
	std::vector<std::string> values_;
	
public:
	ScriptValue_String(const ScriptValue_String &p_original) = default;	// can copy-construct
	ScriptValue_String& operator=(const ScriptValue_String&) = delete;	// no copying
	
	ScriptValue_String(void);
	explicit ScriptValue_String(std::vector<std::string> &p_stringvec);
	explicit ScriptValue_String(const std::string &p_string1);
	ScriptValue_String(const std::string &p_string1, const std::string &p_string2);
	ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3);
	ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4);
	ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5);
	ScriptValue_String(const std::string &p_string1, const std::string &p_string2, const std::string &p_string3, const std::string &p_string4, const std::string &p_string5, const std::string &p_string6);
	virtual ~ScriptValue_String(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	const std::vector<std::string> &StringVector(void) const;
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	void PushString(const std::string &p_string);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	ScriptValue_Int represents integer (C++ int64_t) values in SLiMscript.  The subclass
//	ScriptValue_Int_vector is the standard instance class, used to hold vectors of floats.
//	ScriptValue_Int_singleton_const is used for speed, to represent single constant values.
//

class ScriptValue_Int : public ScriptValue
{
protected:
	ScriptValue_Int(const ScriptValue_Int &p_original) = default;		// can copy-construct
	ScriptValue_Int(void) = default;									// default constructor
	
public:
	ScriptValue_Int& operator=(const ScriptValue_Int&) = delete;		// no copying
	virtual ~ScriptValue_Int(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const = 0;
	
	virtual bool LogicalAtIndex(int p_idx) const = 0;
	virtual std::string StringAtIndex(int p_idx) const = 0;
	virtual int64_t IntAtIndex(int p_idx) const = 0;
	virtual double FloatAtIndex(int p_idx) const = 0;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const = 0;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value) = 0;
	
	virtual ScriptValue *CopyValues(void) const = 0;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value) = 0;
	virtual void Sort(bool p_ascending) = 0;
};

class ScriptValue_Int_vector : public ScriptValue_Int
{
private:
	std::vector<int64_t> values_;
	
public:
	ScriptValue_Int_vector(const ScriptValue_Int_vector &p_original) = default;		// can copy-construct
	ScriptValue_Int_vector& operator=(const ScriptValue_Int_vector&) = delete;	// no copying
	
	ScriptValue_Int_vector(void);
	explicit ScriptValue_Int_vector(std::vector<int> &p_intvec);
	explicit ScriptValue_Int_vector(std::vector<int64_t> &p_intvec);
	//explicit ScriptValue_Int_vector(int64_t p_int1);		// disabled to encourage use of ScriptValue_Int_singleton_const for this case
	ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2);
	ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3);
	ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4);
	ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5);
	ScriptValue_Int_vector(int64_t p_int1, int64_t p_int2, int64_t p_int3, int64_t p_int4, int64_t p_int5, int64_t p_int6);
	virtual ~ScriptValue_Int_vector(void);
	
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	const std::vector<int64_t> &IntVector(void) const;
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	void PushInt(int64_t p_int);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};

class ScriptValue_Int_singleton_const : public ScriptValue_Int
{
private:
	int64_t value_;
	
public:
	ScriptValue_Int_singleton_const(const ScriptValue_Int_singleton_const &p_original) = delete;	// no copy-construct
	ScriptValue_Int_singleton_const& operator=(const ScriptValue_Int_singleton_const&) = delete;	// no copying
	ScriptValue_Int_singleton_const(void) = delete;
	explicit ScriptValue_Int_singleton_const(int64_t p_int1);
	virtual ~ScriptValue_Int_singleton_const(void);
	
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual ScriptValue *CopyValues(void) const;
	
	// prohibited actions
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	ScriptValue_Float represents floating-point (C++ double) values in SLiMscript.  The subclass
//	ScriptValue_Float_vector is the standard instance class, used to hold vectors of floats.
//	ScriptValue_Float_singleton_const is used for speed, to represent single constant values.
//

class ScriptValue_Float : public ScriptValue
{
protected:
	ScriptValue_Float(const ScriptValue_Float &p_original) = default;		// can copy-construct
	ScriptValue_Float(void) = default;										// default constructor
	
public:
	ScriptValue_Float& operator=(const ScriptValue_Float&) = delete;		// no copying
	virtual ~ScriptValue_Float(void);
	
	virtual ScriptValueType Type(void) const;
	virtual int Count(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const = 0;
	
	virtual bool LogicalAtIndex(int p_idx) const = 0;
	virtual std::string StringAtIndex(int p_idx) const = 0;
	virtual int64_t IntAtIndex(int p_idx) const = 0;
	virtual double FloatAtIndex(int p_idx) const = 0;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const = 0;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value) = 0;
	
	virtual ScriptValue *CopyValues(void) const = 0;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value) = 0;
	virtual void Sort(bool p_ascending) = 0;
};

class ScriptValue_Float_vector : public ScriptValue_Float
{
private:
	std::vector<double> values_;
	
public:
	ScriptValue_Float_vector(const ScriptValue_Float_vector &p_original) = default;		// can copy-construct
	ScriptValue_Float_vector& operator=(const ScriptValue_Float_vector&) = delete;	// no copying
	
	ScriptValue_Float_vector(void);
	explicit ScriptValue_Float_vector(std::vector<double> &p_doublevec);
	ScriptValue_Float_vector(double *p_doublebuf, int p_buffer_length);
	//explicit ScriptValue_Float_vector(double p_float1);		// disabled to encourage use of ScriptValue_Float_singleton_const for this case
	ScriptValue_Float_vector(double p_float1, double p_float2);
	ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3);
	ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4);
	ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5);
	ScriptValue_Float_vector(double p_float1, double p_float2, double p_float3, double p_float4, double p_float5, double p_float6);
	virtual ~ScriptValue_Float_vector(void);
	
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	const std::vector<double> &FloatVector(void) const;
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	void PushFloat(double p_float);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};

class ScriptValue_Float_singleton_const : public ScriptValue_Float
{
private:
	double value_;
	
public:
	ScriptValue_Float_singleton_const(const ScriptValue_Float_singleton_const &p_original) = delete;	// no copy-construct
	ScriptValue_Float_singleton_const& operator=(const ScriptValue_Float_singleton_const&) = delete;	// no copying
	ScriptValue_Float_singleton_const(void) = delete;
	explicit ScriptValue_Float_singleton_const(double p_float1);
	virtual ~ScriptValue_Float_singleton_const(void);
	
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual bool LogicalAtIndex(int p_idx) const;
	virtual std::string StringAtIndex(int p_idx) const;
	virtual int64_t IntAtIndex(int p_idx) const;
	virtual double FloatAtIndex(int p_idx) const;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual ScriptValue *CopyValues(void) const;
	
	// prohibited actions
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	ScriptValue_Object represents objects in SLiMscript: entities that have properties and can respond to
//	methods.  The subclass ScriptValue_Object_vector is the standard instance class, used to hold vectors
//	of floats.  ScriptValue_Object_singleton_const is used for speed, to represent single constant values.
//

class ScriptValue_Object : public ScriptValue
{
protected:
	ScriptValue_Object(const ScriptValue_Object &p_original) = default;				// can copy-construct
	ScriptValue_Object(void) = default;												// default constructor
	
public:
	ScriptValue_Object& operator=(const ScriptValue_Object&) = delete;				// no copying
	virtual ~ScriptValue_Object(void);
	
	virtual ScriptValueType Type(void) const;
	virtual const std::string ElementType(void) const = 0;
	virtual int Count(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const = 0;
	
	virtual ScriptObjectElement *ElementAtIndex(int p_idx) const = 0;
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const = 0;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value) = 0;
	
	virtual ScriptValue *CopyValues(void) const = 0;
	virtual ScriptValue *NewMatchingType(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value) = 0;
	virtual void Sort(bool p_ascending);
	
	// Member and method support; defined only on ScriptValue_Object, not ScriptValue.  The methods that a
	// ScriptValue_Object instance defines depend upon the type of the ScriptObjectElement objects it contains.
	virtual std::vector<std::string> ReadOnlyMembersOfElements(void) const = 0;
	virtual std::vector<std::string> ReadWriteMembersOfElements(void) const = 0;
	virtual ScriptValue *GetValueForMemberOfElements(const std::string &p_member_name) const = 0;
	virtual ScriptValue *GetRepresentativeValueOrNullForMemberOfElements(const std::string &p_member_name) const = 0;			// used by code completion
	virtual void SetValueForMemberOfElements(const std::string &p_member_name, ScriptValue *p_value) = 0;
	
	virtual std::vector<std::string> MethodsOfElements(void) const = 0;
	virtual const FunctionSignature *SignatureForMethodOfElements(const std::string &p_method_name) const = 0;
	virtual ScriptValue *ExecuteClassMethodOfElements(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter) = 0;
	virtual ScriptValue *ExecuteInstanceMethodOfElements(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter) = 0;
};

class ScriptValue_Object_vector : public ScriptValue_Object
{
private:
	std::vector<ScriptObjectElement *> values_;		// Whether these are owned or not depends on ScriptObjectElement::ExternallyOwned(); see below
	
public:
	ScriptValue_Object_vector(const ScriptValue_Object_vector &p_original);				// can copy-construct, but it is not the default constructor since we need to deep copy
	ScriptValue_Object_vector& operator=(const ScriptValue_Object_vector&) = delete;		// no copying
	
	ScriptValue_Object_vector(void);
	explicit ScriptValue_Object_vector(std::vector<ScriptObjectElement *> &p_elementvec);
	//explicit ScriptValue_Object_vector(ScriptObjectElement *p_element1);		// disabled to encourage use of ScriptValue_Object_singleton_const for this case
	virtual ~ScriptValue_Object_vector(void);
	
	virtual const std::string ElementType(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual ScriptObjectElement *ElementAtIndex(int p_idx) const;
	void PushElement(ScriptObjectElement *p_element);
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	
	virtual ScriptValue *CopyValues(void) const;
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	void SortBy(const std::string &p_property, bool p_ascending);
	
	// Member and method support; defined only on ScriptValue_Object, not ScriptValue.  The methods that a
	// ScriptValue_Object instance defines depend upon the type of the ScriptObjectElement objects it contains.
	virtual std::vector<std::string> ReadOnlyMembersOfElements(void) const;
	virtual std::vector<std::string> ReadWriteMembersOfElements(void) const;
	virtual ScriptValue *GetValueForMemberOfElements(const std::string &p_member_name) const;
	virtual ScriptValue *GetRepresentativeValueOrNullForMemberOfElements(const std::string &p_member_name) const;			// used by code completion
	virtual void SetValueForMemberOfElements(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> MethodsOfElements(void) const;
	virtual const FunctionSignature *SignatureForMethodOfElements(const std::string &p_method_name) const;
	virtual ScriptValue *ExecuteClassMethodOfElements(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter);
	virtual ScriptValue *ExecuteInstanceMethodOfElements(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter);
};

class ScriptValue_Object_singleton_const : public ScriptValue_Object
{
private:
	ScriptObjectElement *value_;		// Whether these are owned or not depends on ScriptObjectElement::ExternallyOwned(); see below
	
public:
	ScriptValue_Object_singleton_const(const ScriptValue_Object_singleton_const &p_original) = delete;		// no copy-construct
	ScriptValue_Object_singleton_const& operator=(const ScriptValue_Object_singleton_const&) = delete;		// no copying
	ScriptValue_Object_singleton_const(void) = delete;
	explicit ScriptValue_Object_singleton_const(ScriptObjectElement *p_element1);
	virtual ~ScriptValue_Object_singleton_const(void);
	
	virtual const std::string ElementType(void) const;
	virtual int Count(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual ScriptObjectElement *ElementAtIndex(int p_idx) const;
	
	virtual ScriptValue *GetValueAtIndex(const int p_idx) const;
	virtual ScriptValue *CopyValues(void) const;
	
	// prohibited actions
	virtual void SetValueAtIndex(const int p_idx, ScriptValue *p_value);
	virtual void PushValueFromIndexOfScriptValue(int p_idx, const ScriptValue *p_source_script_value);
	
	// Member and method support; defined only on ScriptValue_Object, not ScriptValue.  The methods that a
	// ScriptValue_Object instance defines depend upon the type of the ScriptObjectElement objects it contains.
	virtual std::vector<std::string> ReadOnlyMembersOfElements(void) const;
	virtual std::vector<std::string> ReadWriteMembersOfElements(void) const;
	virtual ScriptValue *GetValueForMemberOfElements(const std::string &p_member_name) const;
	virtual ScriptValue *GetRepresentativeValueOrNullForMemberOfElements(const std::string &p_member_name) const;			// used by code completion
	virtual void SetValueForMemberOfElements(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> MethodsOfElements(void) const;
	virtual const FunctionSignature *SignatureForMethodOfElements(const std::string &p_method_name) const;
	virtual ScriptValue *ExecuteClassMethodOfElements(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter);
	virtual ScriptValue *ExecuteInstanceMethodOfElements(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter);
};


//	*********************************************************************************************************
//
// This is the value type of which ScriptValue_Object is a vector, just as double is the value type of which
// ScriptValue_Float is a vector.  ScriptValue_Object is just a bag; this class is the abstract base class of
// the things that can be contained in that bag.  This class defines the methods that can be used by an
// instance of ScriptValue_Object; ScriptValue_Object just forwards such things on to this class.
// ScriptObjectElement obeys sharing semantics; many ScriptValue_Objects can refer to the same element, and
// ScriptObjectElement never copies itself.  To manage its lifetime, refcounting is used.  Externally-owned
// objects (such as from SLiM) do not use this refcount, since their lifetime is defined externally, but
// internally-owned objects (such as Path) use the refcount and delete themselves when they are done.
class ScriptObjectElement
{
public:
	ScriptObjectElement(const ScriptObjectElement &p_original) = delete;		// can copy-construct
	ScriptObjectElement& operator=(const ScriptObjectElement&) = delete;		// no copying
	
	ScriptObjectElement(void);
	virtual ~ScriptObjectElement(void);
	
	virtual const std::string ElementType(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const;		// standard printing; prints ElementType()
	
	// refcounting; virtual no-ops here (for externally-owned objects), implemented in ScriptObjectElementInternal
	virtual ScriptObjectElement *Retain(void);
	virtual ScriptObjectElement *Release(void);
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name);
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const FunctionSignature *SignatureForMethod(const std::string &p_method_name) const;
	virtual ScriptValue *ExecuteMethod(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter);
	
	// Utility methods for printing errors, checking types, etc.; the goal is to make subclasses as trim as possible
	void TypeCheckValue(const std::string &p_method_name, const std::string &p_member_name, ScriptValue *p_value, ScriptValueMask p_type_mask);
	void RangeCheckValue(const std::string &p_method_name, const std::string &p_member_name, bool p_in_range);
};

std::ostream &operator<<(std::ostream &p_outstream, const ScriptObjectElement &p_element);


// A base class for ScriptObjectElement subclasses that are internal to SLiMscript.  See comment above.
class ScriptObjectElementInternal : public ScriptObjectElement
{
private:
	uint32_t refcount_ = 1;				// start life with a refcount of 1; the allocator does not need to call Retain()
	
public:
	ScriptObjectElementInternal(const ScriptObjectElementInternal &p_original) = delete;		// can copy-construct
	ScriptObjectElementInternal& operator=(const ScriptObjectElementInternal&) = delete;		// no copying
	
	ScriptObjectElementInternal(void);
	virtual ~ScriptObjectElementInternal(void);
	
	virtual ScriptObjectElement *Retain(void);
	virtual ScriptObjectElement *Release(void);
};


#endif /* defined(__SLiM__script_value__) */




































































