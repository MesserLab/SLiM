//
//  eidos_value.h
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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

/*
 
 The class EidosValue represents any variable value in a EidosInterpreter context.  EidosValue itself is an abstract base class.
 Subclasses define types for NULL, logical, string, integer, and float types.  EidosValue types are generally vectors (although
 null is an exception); there are no scalar types in Eidos.
 
 */

#ifndef __Eidos__eidos_value__
#define __Eidos__eidos_value__


#include <iostream>
#include <vector>
#include <string>
#include <initializer_list>

#include "eidos_global.h"
#include "eidos_intrusive_ptr.h"
#include "eidos_object_pool.h"
#include "eidos_script.h"


class EidosValue;
class EidosValue_NULL;
class EidosValue_Logical;
class EidosValue_Logical_const;
class EidosValue_Int;
class EidosValue_Int_singleton;
class EidosValue_Int_vector;
class EidosValue_Float;
class EidosValue_Float_singleton;
class EidosValue_Float_vector;
class EidosValue_String;
class EidosValue_String_singleton;
class EidosValue_String_vector;
class EidosValue_Object;
class EidosValue_Object_singleton;
class EidosValue_Object_vector;

class EidosObjectPool;
class EidosPropertySignature;
class EidosFunctionSignature;
class EidosMethodSignature;
class EidosInterpreter;
class EidosToken;

class EidosObjectElement;	// the value type for EidosValue_Object; defined at the bottom of this file
class EidosObjectClass;		// the class definition object for EidosObjectElement; also defined at bottom


// Type int64_t is used for Eidos "integer", type double is used for Eidos "float", type std::string is used
// for Eidos "string", and EidosObjectElement* is used for Eidos "object".  The type used for Eidos "logical"
// is a bit less clear, and so is controlled by a typedef here.  Type bool would be the obvious choice, but
// std::vector<bool> is a special class that may not be desirable; it generally encapsulates a priority for
// small memory usage over high speed, which is not our priority.  The measured speed difference is not large,
// about 4% for fairly logical-intensive tests, but the memory penalty seems irrelevant, so why not.  If you
// feel differently, switching the typedef below should cleanly switch over to bool instead of uint8_t.
typedef uint8_t eidos_logical_t;
//typedef bool eidos_logical_t;


// We use Eidos_intrusive_ptr to refer to most EidosValue instances, unless they are used only in one place with
// a single owner.  For convenience, there is a typedef for Eidos_intrusive_ptr for each EidosValue subclass.
typedef Eidos_intrusive_ptr<EidosValue>						EidosValue_SP;
typedef Eidos_intrusive_ptr<EidosValue_NULL>				EidosValue_NULL_SP;
typedef Eidos_intrusive_ptr<EidosValue_Logical>				EidosValue_Logical_SP;
typedef Eidos_intrusive_ptr<EidosValue_Logical_const>		EidosValue_Logical_const_SP;
typedef Eidos_intrusive_ptr<EidosValue_Int>					EidosValue_Int_SP;
typedef Eidos_intrusive_ptr<EidosValue_Int_singleton>		EidosValue_Int_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_Int_vector>			EidosValue_Int_vector_SP;
typedef Eidos_intrusive_ptr<EidosValue_Float>				EidosValue_Float_SP;
typedef Eidos_intrusive_ptr<EidosValue_Float_singleton>		EidosValue_Float_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_Float_vector>		EidosValue_Float_vector_SP;
typedef Eidos_intrusive_ptr<EidosValue_String>				EidosValue_String_SP;
typedef Eidos_intrusive_ptr<EidosValue_String_singleton>	EidosValue_String_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_String_vector>		EidosValue_String_vector_SP;
typedef Eidos_intrusive_ptr<EidosValue_Object>				EidosValue_Object_SP;
typedef Eidos_intrusive_ptr<EidosValue_Object_singleton>	EidosValue_Object_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_Object_vector>		EidosValue_Object_vector_SP;


// EidosValues must be allocated out of the global pool, for speed.  See eidos_object_pool.h.  When Eidos disposes of an object,
// it will assume that it was allocated from this pool, so its use is mandatory except for stack-allocated objects.
extern EidosObjectPool *gEidosValuePool;


// Global EidosValues that are defined at Eidos_WarmUp() time and are never deallocated.
extern EidosValue_NULL_SP gStaticEidosValueNULL;
extern EidosValue_NULL_SP gStaticEidosValueNULLInvisible;

extern EidosValue_Logical_SP gStaticEidosValue_Logical_ZeroVec;
extern EidosValue_Int_SP gStaticEidosValue_Integer_ZeroVec;
extern EidosValue_Float_SP gStaticEidosValue_Float_ZeroVec;
extern EidosValue_String_SP gStaticEidosValue_String_ZeroVec;
extern EidosValue_Object_SP gStaticEidosValue_Object_ZeroVec;

extern EidosValue_Logical_SP gStaticEidosValue_LogicalT;
extern EidosValue_Logical_SP gStaticEidosValue_LogicalF;

extern EidosValue_Int_SP gStaticEidosValue_Integer0;
extern EidosValue_Int_SP gStaticEidosValue_Integer1;

extern EidosValue_Float_SP gStaticEidosValue_Float0;
extern EidosValue_Float_SP gStaticEidosValue_Float0Point5;
extern EidosValue_Float_SP gStaticEidosValue_Float1;
extern EidosValue_Float_SP gStaticEidosValue_FloatINF;

extern EidosValue_String_SP gStaticEidosValue_StringEmpty;
extern EidosValue_String_SP gStaticEidosValue_StringSpace;
extern EidosValue_String_SP gStaticEidosValue_StringAsterisk;
extern EidosValue_String_SP gStaticEidosValue_StringDoubleAsterisk;


// EidosValueType is an enum of the possible types for EidosValue objects.  Note that all of these types are vectors of the stated
// type; all objects in Eidos are vectors.  The order of these types is in type-promotion order, from lowest to highest, except
// that NULL never gets promoted to any other type, and nothing ever gets promoted to object type.
enum class EidosValueType : uint8_t
{
	kValueNULL = 0,		// special NULL type; this cannot be mixed with other types or promoted to other types
	
	kValueLogical,		// logicals (eidos_logical_t)
	kValueInt,			// integers (int64_t)
	kValueFloat,		// floats (double)
	kValueString,		// strings (std:string)
	
	kValueObject		// a vector of EidosObjectElement objects: these represent built-in objects with properties and methods
};

std::string StringForEidosValueType(const EidosValueType p_type);
std::ostream &operator<<(std::ostream &p_outstream, const EidosValueType p_type);


// Comparing values is a bit complex, because we want it to be as fast as possible, but there are issues involved,
// particularly type promotion; you can compare a string to integer, or less extremely, a float to an integer, and
// the appropriate promotion of values needs to happen.  The first function here handles the general case; the
// other functions allow optimization in bottlenecks.  Even more optimization is possible using type-specific
// methods.  Returns -1 if value1[index1] < value2[index2], 0 if ==, 1 if value1[index1] > value2[index2].
int CompareEidosValues(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token);

int CompareEidosValues_Object(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token);
int CompareEidosValues_String(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token);
int CompareEidosValues_Float(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token);
int CompareEidosValues_Int(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token);
int CompareEidosValues_Logical(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token);

typedef int (*EidosCompareFunctionPtr)(const EidosValue &p_value1, int p_index1, const EidosValue &p_value2, int p_index2, const EidosToken *p_blame_token);

EidosCompareFunctionPtr EidosGetCompareFunctionForTypes(EidosValueType p_type1, EidosValueType p_type2, const EidosToken *p_blame_token);


// EidosValueMask is a uint32_t used as a bit mask to identify permitted types for EidosValue objects (arguments, returns)
// Note that these mask values must correspond to the values in EidosValueType directly; (1 << (int)type) == mask must be true.
typedef uint32_t EidosValueMask;

const EidosValueMask kEidosValueMaskNone =			0x00000000;
const EidosValueMask kEidosValueMaskNULL =			0x00000001;
const EidosValueMask kEidosValueMaskLogical =		0x00000002;
const EidosValueMask kEidosValueMaskInt =			0x00000004;
const EidosValueMask kEidosValueMaskFloat =			0x00000008;
const EidosValueMask kEidosValueMaskString =		0x00000010;
const EidosValueMask kEidosValueMaskObject =		0x00000020;
	
const EidosValueMask kEidosValueMaskOptional =		0x80000000;
const EidosValueMask kEidosValueMaskSingleton =		0x40000000;
const EidosValueMask kEidosValueMaskFlagStrip =		0x3FFFFFFF;
	
const EidosValueMask kEidosValueMaskNumeric =		(kEidosValueMaskInt | kEidosValueMaskFloat);									// integer or float
const EidosValueMask kEidosValueMaskLogicalEquiv =	(kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskFloat);			// logical, integer, or float
const EidosValueMask kEidosValueMaskAnyBase =		(kEidosValueMaskNULL | kEidosValueMaskLogicalEquiv | kEidosValueMaskString);	// any type except object
const EidosValueMask kEidosValueMaskAny =			(kEidosValueMaskAnyBase | kEidosValueMaskObject);								// any type including object

std::string StringForEidosValueMask(const EidosValueMask p_mask, const EidosObjectClass *p_object_class, const std::string &p_name, EidosValue *p_default);
//std::ostream &operator<<(std::ostream &p_outstream, const EidosValueMask p_mask);	// can't do this since EidosValueMask is just uint32_t


//	*********************************************************************************************************
//
//	A class representing a value resulting from script evaluation.  Eidos is quite dynamically typed;
//	problems cause runtime exceptions.  EidosValue is the abstract base class for all value classes.
//

class EidosValue
{
	//	This class has its assignment operator disabled, to prevent accidental copying.
protected:
	
	mutable uint32_t intrusive_ref_count;					// used by Eidos_intrusive_ptr
	const EidosValueType cached_type_;						// allows Type() to be an inline function; cached at construction
	uint8_t invisible_;										// as in R; if true, the value will not normally be printed to the console
	uint8_t is_singleton_;									// allows Count() and IsSingleton() to be inline; cached at construction
	
public:
	
	EidosValue(const EidosValue &p_original) = delete;		// no copy-construct
	EidosValue& operator=(const EidosValue&) = delete;		// no copying
	
	EidosValue(void) = delete;									// no null constructor
	EidosValue(EidosValueType p_value_type, bool p_singleton);	// must construct with a type identifier and singleton flag, which will be cached
	virtual ~EidosValue(void) = 0;								// virtual destructor
	
	// basic methods
	inline __attribute__((always_inline)) EidosValueType Type(void) const { return cached_type_; }	// the type of the vector, cached at construction
	inline __attribute__((always_inline)) bool IsSingleton(void) const { return is_singleton_; }	// true is the subclass is a singleton subclass (not just if Count()==1)
	inline __attribute__((always_inline)) int Count(void) const { return (is_singleton_ ? 1 : Count_Virtual()); }	// avoid the virtual function call for singletons
	
	virtual const std::string &ElementType(void) const = 0;	// the type of the elements contained by the vector
	virtual int Count_Virtual(void) const = 0;				// the number of values in the vector
	virtual void Print(std::ostream &p_ostream) const = 0;	// standard printing
	
	// getter only; invisible objects must be made through construction or InvisibleCopy()
	inline __attribute__((always_inline)) bool Invisible(void) const							{ return invisible_; }
	
	// basic subscript access; abstract here since we want to force subclasses to define this
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token) = 0;
	
	// fetching individual values; these convert type if necessary, and (base class behavior) raise if impossible
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	// methods to allow type-agnostic manipulation of EidosValues
	virtual EidosValue_SP VectorBasedCopy(void) const;				// just calls CopyValues() by default, but guarantees a mutable copy
	virtual EidosValue_SP CopyValues(void) const = 0;				// a deep copy of the receiver with invisible_ == false
	virtual EidosValue_SP NewMatchingType(void) const = 0;			// a new EidosValue instance of the same type as the receiver
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token) = 0;	// copy a value
	virtual void Sort(bool p_ascending) = 0;
	
	// Methods to get a type-specific vector for fast manipulation; these raise if called on a value that does not support them.
	// This is much faster than using dynamic_cast to achieve the same effect, and much safer than using static_cast + non-virtual function.
	// Note that these methods are conventionally used in Eidos by immediately dereferencing to create a reference to the vector; this
	// should be safe, since these methods should always raise rather than returning nullptr.
	void RaiseForUnimplementedVectorCall(void) const __attribute__((__noreturn__));
	virtual const std::vector<eidos_logical_t> *LogicalVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual std::vector<eidos_logical_t> *LogicalVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const std::vector<std::string> *StringVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual std::vector<std::string> *StringVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const std::vector<int64_t> *IntVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual std::vector<int64_t> *IntVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const std::vector<double> *FloatVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual std::vector<double> *FloatVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const std::vector<EidosObjectElement *> *ObjectElementVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual std::vector<EidosObjectElement *> *ObjectElementVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	
	// Eidos_intrusive_ptr support; we use Eidos_intrusive_ptr as a fast smart pointer to EidosValue.
	inline __attribute__((always_inline)) uint32_t use_count() const { return intrusive_ref_count; }
	inline __attribute__((always_inline)) bool unique() const { return intrusive_ref_count == 1; }
	inline __attribute__((always_inline)) void stack_allocated() { intrusive_ref_count++; }			// used with stack-allocated EidosValues that have to be put under Eidos_intrusive_ptr
	
	friend void Eidos_intrusive_ptr_add_ref(const EidosValue *p_value);
	friend void Eidos_intrusive_ptr_release(const EidosValue *p_value);
	
	// This is a hack scheme to track EidosValue allocations and deallocations, as a way to help debug leaks
	// To enable it, change the #under to #define.  When you run Eidos tests, a summary of persistent EidosValues will print.
	// The standard global values should be persistent (T, F, INF, NAN, NULL, NULL-invisible, E, PI); all others should be freed.
#undef EIDOS_TRACK_VALUE_ALLOCATION
	
#ifdef EIDOS_TRACK_VALUE_ALLOCATION
	static int valueTrackingCount;
	static std::vector<EidosValue *> valueTrackingVector;
#endif
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosValue &p_value);

// Eidos_intrusive_ptr support
inline __attribute__((always_inline)) void Eidos_intrusive_ptr_add_ref(const EidosValue *p_value)
{
	++(p_value->intrusive_ref_count);
}

inline __attribute__((always_inline)) void Eidos_intrusive_ptr_release(const EidosValue *p_value)
{
	if ((--(p_value->intrusive_ref_count)) == 0)
	{
		// We no longer delete; all EidosValues under Eidos_intrusive_ptr should have been allocated out of gEidosValuePool, so it handles the free
		//delete p_value;
		p_value->~EidosValue();
		gEidosValuePool->DisposeChunk(const_cast<EidosValue*>(p_value));
	}
}


//	*********************************************************************************************************
//
//	EidosValue_NULL and EidosValue_NULL_const represent NULL values in Eidos.  EidosValue_NULL_const
//	is used for static global EidosValue_NULL instances; we have two, one invisible, one not.
//

class EidosValue_NULL : public EidosValue
{
public:
	EidosValue_NULL(const EidosValue_NULL &p_original) = delete;	// no copy-construct
	EidosValue_NULL& operator=(const EidosValue_NULL&) = delete;	// no copying
	
	EidosValue_NULL(void) : EidosValue(EidosValueType::kValueNULL, false) { }
	virtual ~EidosValue_NULL(void);
	
	static EidosValue_NULL_SP Static_EidosValue_NULL(void);
	static EidosValue_NULL_SP Static_EidosValue_NULL_Invisible(void);
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);

	virtual EidosValue_SP CopyValues(void) const;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	EidosValue_Logical and EidosValue_Logical_const represent logical (bool) values in Eidos.
//	EidosValue_Logical_const is used for static global EidosValue_Logical instances; we have two, T and F.
//	Because there are only two values, T and F, we don't need a singleton class, we just use those globals.
//

class EidosValue_Logical : public EidosValue
{
protected:
	std::vector<eidos_logical_t> values_;
	
protected:
	explicit EidosValue_Logical(eidos_logical_t p_logical1);		// protected to encourage use of EidosValue_Logical_const for this
	
public:
	EidosValue_Logical(const EidosValue_Logical &p_original) = delete;	// no copy-construct
	EidosValue_Logical& operator=(const EidosValue_Logical&) = delete;	// no copying
	
	EidosValue_Logical(void);
	explicit EidosValue_Logical(const std::vector<eidos_logical_t> &p_logicalvec);
	explicit EidosValue_Logical(std::initializer_list<eidos_logical_t> p_init_list);
	virtual ~EidosValue_Logical(void);
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual const std::vector<eidos_logical_t> *LogicalVector(void) const { return &values_; }
	virtual std::vector<eidos_logical_t> *LogicalVector_Mutable(void) { return &values_; }
	virtual EidosValue_Logical *Reserve(int p_reserved_size);	// returns this, for chaining with new
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual void PushLogical(eidos_logical_t p_logical);
	virtual void SetLogicalAtIndex(const int p_idx, eidos_logical_t p_logical, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};

class EidosValue_Logical_const : public EidosValue_Logical
{
public:
	EidosValue_Logical_const(const EidosValue_Logical_const &p_original) = delete;	// no copy-construct
	EidosValue_Logical_const(void) = delete;										// no default constructor
	EidosValue_Logical_const& operator=(const EidosValue_Logical_const&) = delete;	// no copying
	explicit EidosValue_Logical_const(eidos_logical_t p_logical1);
	virtual ~EidosValue_Logical_const(void);											// destructor calls eidos_terminate()
	
	static EidosValue_Logical_SP Static_EidosValue_Logical_T(void);
	static EidosValue_Logical_SP Static_EidosValue_Logical_F(void);
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because this subclass represents only truly immutable objects
	virtual std::vector<eidos_logical_t> *LogicalVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual EidosValue_Logical *Reserve(int p_reserved_size);
	virtual void PushLogical(eidos_logical_t p_logical);
	virtual void SetLogicalAtIndex(const int p_idx, eidos_logical_t p_logical, const EidosToken *p_blame_token);
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	EidosValue_String represents string (C++ std::string) values in Eidos.  The subclass
//	EidosValue_String_vector is the standard instance class, used to hold vectors of strings.
//	EidosValue_String_singleton is used for speed, to represent single values.
//

class EidosValue_String : public EidosValue
{
protected:
	EidosValue_String(bool p_singleton) : EidosValue(EidosValueType::kValueString, p_singleton) {}
	
public:
	EidosValue_String(const EidosValue_String &p_original) = delete;		// no copy-construct
	EidosValue_String(void) = delete;										// no default constructor
	EidosValue_String& operator=(const EidosValue_String&) = delete;		// no copying
	virtual ~EidosValue_String(void);
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const = 0;
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token) = 0;
	
	virtual EidosValue_SP CopyValues(void) const = 0;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token) = 0;
	virtual void Sort(bool p_ascending) = 0;
};

class EidosValue_String_vector : public EidosValue_String
{
protected:
	std::vector<std::string> values_;
	
public:
	EidosValue_String_vector(const EidosValue_String_vector &p_original) = delete;	// no copy-construct
	EidosValue_String_vector& operator=(const EidosValue_String_vector&) = delete;	// no copying
	
	EidosValue_String_vector(void);
	explicit EidosValue_String_vector(const std::vector<std::string> &p_stringvec);
	EidosValue_String_vector(double *p_doublebuf, int p_buffer_length);
	//explicit EidosValue_String_vector(const std::string &p_string1);		// disabled to encourage use of EidosValue_String_singleton for this case
	explicit EidosValue_String_vector(std::initializer_list<const std::string> p_init_list);
	virtual ~EidosValue_String_vector(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual const std::vector<std::string> *StringVector(void) const { return &values_; }
	virtual std::vector<std::string> *StringVector_Mutable(void) { return &values_; }
	inline __attribute__((always_inline)) void PushString(const std::string &p_string) { values_.emplace_back(p_string); }
	inline __attribute__((always_inline)) EidosValue_String_vector *Reserve(int p_reserved_size) { values_.reserve(p_reserved_size); return this; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};

class EidosValue_String_singleton : public EidosValue_String
{
protected:
	std::string value_;
	EidosScript *cached_script_ = nullptr;	// cached by executeLambda() and apply() to avoid multiple tokenize/parse overhead
	
public:
	EidosValue_String_singleton(const EidosValue_String_singleton &p_original) = delete;	// no copy-construct
	EidosValue_String_singleton& operator=(const EidosValue_String_singleton&) = delete;	// no copying
	EidosValue_String_singleton(void) = delete;
	explicit EidosValue_String_singleton(const std::string &p_string1);
	virtual ~EidosValue_String_singleton(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	inline __attribute__((always_inline)) std::string &StringValue_Mutable(void) { delete cached_script_; cached_script_ = nullptr; return value_; }
	inline __attribute__((always_inline)) void SetValue(const std::string &p_string) { delete cached_script_; cached_script_ = nullptr; value_ = p_string; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosValue_SP CopyValues(void) const;
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because there is no backing vector
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
	
	// script caching; this is something that only EidosValue_String_singleton does!
	inline __attribute__((always_inline)) EidosScript *CachedScript(void) { return cached_script_; }
	inline __attribute__((always_inline)) void SetCachedScript(EidosScript *p_script) { cached_script_ = p_script; }
};


//	*********************************************************************************************************
//
//	EidosValue_Int represents integer (C++ int64_t) values in Eidos.  The subclass
//	EidosValue_Int_vector is the standard instance class, used to hold vectors of floats.
//	EidosValue_Int_singleton is used for speed, to represent single values.
//

class EidosValue_Int : public EidosValue
{
protected:
	EidosValue_Int(bool p_singleton) : EidosValue(EidosValueType::kValueInt, p_singleton) {}
	
public:
	EidosValue_Int(const EidosValue_Int &p_original) = delete;			// no copy-construct
	EidosValue_Int(void) = delete;										// no default constructor
	EidosValue_Int& operator=(const EidosValue_Int&) = delete;			// no copying
	virtual ~EidosValue_Int(void);
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const = 0;
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token) = 0;
	
	virtual EidosValue_SP CopyValues(void) const = 0;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token) = 0;
	virtual void Sort(bool p_ascending) = 0;
};

class EidosValue_Int_vector : public EidosValue_Int
{
protected:
	std::vector<int64_t> values_;
	
public:
	EidosValue_Int_vector(const EidosValue_Int_vector &p_original) = delete;	// no copy-construct
	EidosValue_Int_vector& operator=(const EidosValue_Int_vector&) = delete;	// no copying
	
	EidosValue_Int_vector(void);
	explicit EidosValue_Int_vector(const std::vector<int16_t> &p_intvec);
	explicit EidosValue_Int_vector(const std::vector<int32_t> &p_intvec);
	explicit EidosValue_Int_vector(const std::vector<int64_t> &p_intvec);
	//explicit EidosValue_Int_vector(int64_t p_int1);		// disabled to encourage use of EidosValue_Int_singleton for this case
	explicit EidosValue_Int_vector(std::initializer_list<int64_t> p_init_list);
	virtual ~EidosValue_Int_vector(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual const std::vector<int64_t> *IntVector(void) const { return &values_; }
	virtual std::vector<int64_t> *IntVector_Mutable(void) { return &values_; }
	inline __attribute__((always_inline)) void PushInt(int64_t p_int) { values_.emplace_back(p_int); }
	inline __attribute__((always_inline)) EidosValue_Int_vector *Reserve(int p_reserved_size) { values_.reserve(p_reserved_size); return this; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};

class EidosValue_Int_singleton : public EidosValue_Int
{
protected:
	int64_t value_;
	
public:
	EidosValue_Int_singleton(const EidosValue_Int_singleton &p_original) = delete;	// no copy-construct
	EidosValue_Int_singleton& operator=(const EidosValue_Int_singleton&) = delete;	// no copying
	EidosValue_Int_singleton(void) = delete;
	explicit EidosValue_Int_singleton(int64_t p_int1);
	virtual ~EidosValue_Int_singleton(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	inline __attribute__((always_inline)) int64_t &IntValue_Mutable(void) { return value_; }
	inline __attribute__((always_inline)) void SetValue(int64_t p_int) { value_ = p_int; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosValue_SP CopyValues(void) const;
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because there is no backing vector
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	EidosValue_Float represents floating-point (C++ double) values in Eidos.  The subclass
//	EidosValue_Float_vector is the standard instance class, used to hold vectors of floats.
//	EidosValue_Float_singleton is used for speed, to represent single values.
//

class EidosValue_Float : public EidosValue
{
protected:
	EidosValue_Float(bool p_singleton) : EidosValue(EidosValueType::kValueFloat, p_singleton) {}
	
public:
	EidosValue_Float(const EidosValue_Float &p_original) = delete;			// no copy-construct
	EidosValue_Float(void) = delete;										// no default constructor
	EidosValue_Float& operator=(const EidosValue_Float&) = delete;			// no copying
	virtual ~EidosValue_Float(void);
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const = 0;
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token) = 0;
	
	virtual EidosValue_SP CopyValues(void) const = 0;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token) = 0;
	virtual void Sort(bool p_ascending) = 0;
};

class EidosValue_Float_vector : public EidosValue_Float
{
protected:
	std::vector<double> values_;
	
public:
	EidosValue_Float_vector(const EidosValue_Float_vector &p_original) = delete;	// no copy-construct
	EidosValue_Float_vector& operator=(const EidosValue_Float_vector&) = delete;	// no copying
	
	EidosValue_Float_vector(void);
	explicit EidosValue_Float_vector(const std::vector<double> &p_doublevec);
	EidosValue_Float_vector(double *p_doublebuf, int p_buffer_length);
	//explicit EidosValue_Float_vector(double p_float1);		// disabled to encourage use of EidosValue_Float_singleton for this case
	explicit EidosValue_Float_vector(std::initializer_list<double> p_init_list);
	virtual ~EidosValue_Float_vector(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual const std::vector<double> *FloatVector(void) const { return &values_; }
	virtual std::vector<double> *FloatVector_Mutable(void) { return &values_; }
	inline __attribute__((always_inline)) void PushFloat(double p_float) { values_.emplace_back(p_float); }
	inline __attribute__((always_inline)) EidosValue_Float_vector *Reserve(int p_reserved_size) { values_.reserve(p_reserved_size); return this; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};

class EidosValue_Float_singleton : public EidosValue_Float
{
protected:
	double value_;
	
public:
	EidosValue_Float_singleton(const EidosValue_Float_singleton &p_original) = delete;	// no copy-construct
	EidosValue_Float_singleton& operator=(const EidosValue_Float_singleton&) = delete;	// no copying
	EidosValue_Float_singleton(void) = delete;
	explicit EidosValue_Float_singleton(double p_float1);
	virtual ~EidosValue_Float_singleton(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	inline __attribute__((always_inline)) double &FloatValue_Mutable(void) { return value_; }
	inline __attribute__((always_inline)) void SetValue(double p_float) { value_ = p_float; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosValue_SP CopyValues(void) const;
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because there is no backing vector
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


//	*********************************************************************************************************
//
//	EidosValue_Object represents objects in Eidos: entities that have properties and can respond to
//	methods.  The subclass EidosValue_Object_vector is the standard instance class, used to hold vectors
//	of objects.  EidosValue_Object_singleton is used for speed, to represent single values.
//

class EidosValue_Object : public EidosValue
{
protected:
	const EidosObjectClass *class_;		// can be gEidos_UndefinedClassObject if the vector is empty
	
	EidosValue_Object(bool p_singleton, const EidosObjectClass *p_class) : EidosValue(EidosValueType::kValueObject, p_singleton), class_(p_class) {}
	
public:
	EidosValue_Object(const EidosValue_Object &p_original) = delete;				// no copy-construct
	EidosValue_Object(void) = delete;												// no default constructor
	EidosValue_Object& operator=(const EidosValue_Object&) = delete;				// no copying
	virtual ~EidosValue_Object(void);
	
	virtual const std::string &ElementType(void) const;
	inline __attribute__((always_inline)) const EidosObjectClass *Class(void) const { return class_; }
	virtual int Count_Virtual(void) const = 0;
	virtual void Print(std::ostream &p_ostream) const = 0;
	
	virtual EidosObjectElement *ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token) = 0;
	
	virtual EidosValue_SP CopyValues(void) const = 0;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token) = 0;
	virtual void Sort(bool p_ascending);
	
	// Property and method support; defined only on EidosValue_Object, not EidosValue.  The methods that a
	// EidosValue_Object instance defines depend upon the type of the EidosObjectElement objects it contains.
	virtual EidosValue_SP GetPropertyOfElements(EidosGlobalStringID p_property_id) const = 0;
	virtual void SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value) = 0;
	
	virtual EidosValue_SP ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosMethodSignature *p_call_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) = 0;
};

class EidosValue_Object_vector : public EidosValue_Object
{
protected:
	std::vector<EidosObjectElement *> values_;		// these use a retain/release system of ownership; see below
	
public:
	EidosValue_Object_vector(const EidosValue_Object_vector &p_original);				// can copy-construct
	EidosValue_Object_vector& operator=(const EidosValue_Object_vector&) = delete;		// no copying
	
	explicit EidosValue_Object_vector(const EidosObjectClass *p_class);							// can be gEidos_UndefinedClassObject
	explicit EidosValue_Object_vector(const std::vector<EidosObjectElement *> &p_elementvec, const EidosObjectClass *p_class);
	//explicit EidosValue_Object_vector(EidosObjectElement *p_element1);		// disabled to encourage use of EidosValue_Object_singleton for this case
	explicit EidosValue_Object_vector(std::initializer_list<EidosObjectElement *> p_init_list, const EidosObjectClass *p_class);
	virtual ~EidosValue_Object_vector(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosObjectElement *ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual const std::vector<EidosObjectElement *> *ObjectElementVector(void) const { return &values_; }
	virtual std::vector<EidosObjectElement *> *ObjectElementVector_Mutable(void) { return &values_; }
	void PushObjectElement(EidosObjectElement *p_element);
	inline __attribute__((always_inline)) EidosValue_Object_vector *Reserve(int p_reserved_size) { values_.reserve(p_reserved_size); return this; }
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	void SortBy(const std::string &p_property, bool p_ascending);
	
	// Property and method support; defined only on EidosValue_Object, not EidosValue.  The methods that a
	// EidosValue_Object instance defines depend upon the type of the EidosObjectElement objects it contains.
	virtual EidosValue_SP GetPropertyOfElements(EidosGlobalStringID p_property_id) const;
	virtual void SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosMethodSignature *p_call_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};

class EidosValue_Object_singleton : public EidosValue_Object
{
protected:
	EidosObjectElement *value_;		// these use a retain/release system of ownership; see below
	
public:
	EidosValue_Object_singleton(const EidosValue_Object_singleton &p_original) = delete;		// no copy-construct
	EidosValue_Object_singleton& operator=(const EidosValue_Object_singleton&) = delete;		// no copying
	EidosValue_Object_singleton(void) = delete;
	explicit EidosValue_Object_singleton(EidosObjectElement *p_element1, const EidosObjectClass *p_class);
	virtual ~EidosValue_Object_singleton(void);
	
	virtual int Count_Virtual(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosObjectElement *ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	inline __attribute__((always_inline)) EidosObjectElement * &ObjectElementValue_Mutable(void) { return value_; }	// don't forget Retain()/Relase() on the elements!
	void SetValue(EidosObjectElement *p_element);
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosValue_SP CopyValues(void) const;
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because there is no backing vector
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	
	// Property and method support; defined only on EidosValue_Object, not EidosValue.  The methods that a
	// EidosValue_Object instance defines depend upon the type of the EidosObjectElement objects it contains.
	virtual EidosValue_SP GetPropertyOfElements(EidosGlobalStringID p_property_id) const;
	virtual void SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosMethodSignature *p_call_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


//	*********************************************************************************************************
//
// This is the value type of which EidosValue_Object is a vector, just as double is the value type of which
// EidosValue_Float is a vector.  EidosValue_Object is just a bag; this class is the abstract base class of
// the things that can be contained in that bag.  This class defines the methods that can be used by an
// instance of EidosValue_Object; EidosValue_Object just forwards such things on to this class.
// EidosObjectElement obeys sharing semantics; many EidosValue_Objects can refer to the same element, and
// EidosObjectElement never copies itself.  To manage its lifetime, refcounting is used.  Externally-owned
// objects (from the Context) do not use this refcount, since their lifetime is defined externally, but
// internally-owned objects (such as _Test, the only internally-owned object type at present) use the
// refcount and delete themselves when they are done.

class EidosObjectElement
{
public:
	EidosObjectElement(const EidosObjectElement &p_original) = delete;		// no copy-construct
	EidosObjectElement& operator=(const EidosObjectElement&) = delete;		// no copying
	
	EidosObjectElement(void);
	virtual ~EidosObjectElement(void);
	
	virtual const EidosObjectClass *Class(void) const = 0;
	
	virtual void Print(std::ostream &p_ostream) const;		// standard printing; prints ElementType()
	
	// refcounting; virtual no-ops here (for externally-owned objects), implemented in EidosObjectElementInternal
	virtual EidosObjectElement *Retain(void);
	virtual EidosObjectElement *Release(void);
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access, by returning raw C++ types.  Participation in this scheme is entirely optional
	// on the part of subclasses; you can just implement GetProperty() if you wish.  If you do choose to opt in to
	// acceleration, you must still implement GetProperty() fully as well.  Opt in by calling DeclareAcceleratedGet()
	// on the property signatures that you wish to accelerate, in your class interface code.  Only singleton
	// properties with a single, fully declared return type may be accelerated.
	virtual eidos_logical_t GetProperty_Accelerated_Logical(EidosGlobalStringID p_property_id);
	virtual int64_t GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id);
	virtual double GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id);
	virtual std::string GetProperty_Accelerated_String(EidosGlobalStringID p_property_id);
	virtual EidosObjectElement *GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id);
	
	// Accelerated property writing, by receiving raw C++ types.  As with accelerated writing, participation in
	// this scheme is optional.  Opt in by calling DeclareAcceleratedSet() on the signatures you wish to accelerate.
	// Only singleton read-write properties with a single, fully declared type may be accelerated.
	virtual void SetProperty_Accelerated_Logical(EidosGlobalStringID p_property_id, eidos_logical_t p_value);
	virtual void SetProperty_Accelerated_Int(EidosGlobalStringID p_property_id, int64_t p_value);
	virtual void SetProperty_Accelerated_Float(EidosGlobalStringID p_property_id, double p_value);
	virtual void SetProperty_Accelerated_String(EidosGlobalStringID p_property_id, const std::string &p_value);
	virtual void SetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id, const EidosObjectElement *p_value);
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosObjectElement &p_element);


// A base class for EidosObjectElement subclasses that are internal to Eidos.  See comment above.
class EidosObjectElementInternal : public EidosObjectElement
{
private:
	uint32_t refcount_ = 1;				// start life with a refcount of 1; the allocator does not need to call Retain()
	
public:
	EidosObjectElementInternal(const EidosObjectElementInternal &p_original) = delete;		// no copy-construct
	EidosObjectElementInternal& operator=(const EidosObjectElementInternal&) = delete;		// no copying
	
	EidosObjectElementInternal(void);
	virtual ~EidosObjectElementInternal(void);
	
	virtual EidosObjectElement *Retain(void);
	virtual EidosObjectElement *Release(void);
};


//	*********************************************************************************************************
//
// This class is similar to Class objects in Objective-C; it represents the class of an EidosObjectElement.
// These class objects are not visible in Eidos, at least at present; they just work behind the scenes to
// enable some behaviors.  In particular: (1) class objects define the interface, of methods and properties,
// that elements implement; (2) class objects implement class methods, avoiding the need to call class
// methods on an arbitrarily chosen instance of an element type; and most importantly, (3) class objects exist
// even when no instance exists, allowing facilities like code completion to handle types and interfaces
// without having to have instantiated object elements in hand.  It is conceivable that class objects will
// become visible in Eidos at some point; but there seems to be no need for that at present.
//
// Note that this is not an abstract base class!  It is used to represent the class of empty objects.

class EidosObjectClass
{
public:
	EidosObjectClass(const EidosObjectClass &p_original) = delete;		// no copy-construct
	EidosObjectClass& operator=(const EidosObjectClass&) = delete;		// no copying
	
	EidosObjectClass(void);
	virtual ~EidosObjectClass(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	
	// These non-virtual utility methods do the same as SignatureForProperty() / SignatureForMethod() but raise on failure
	const EidosPropertySignature *SignatureForPropertyOrRaise(EidosGlobalStringID p_property_id) const;
	const EidosMethodSignature *SignatureForMethodOrRaise(EidosGlobalStringID p_method_id) const;
};

extern EidosObjectClass *gEidos_UndefinedClassObject;


#endif /* defined(__Eidos__eidos_value__) */




































































