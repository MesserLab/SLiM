//
//  eidos_value.h
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
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
#include <unordered_map>

#include "eidos_global.h"
#include "eidos_intrusive_ptr.h"
#include "eidos_object_pool.h"
#include "eidos_script.h"


class EidosValue;
class EidosValue_VOID;
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
class EidosMethodSignature;
class EidosInstanceMethodSignature;
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
typedef Eidos_intrusive_ptr<EidosValue_VOID>				EidosValue_VOID_SP;
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
extern EidosValue_VOID_SP gStaticEidosValueVOID;

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
extern EidosValue_Int_SP gStaticEidosValue_Integer2;
extern EidosValue_Int_SP gStaticEidosValue_Integer3;

extern EidosValue_Float_SP gStaticEidosValue_Float0;
extern EidosValue_Float_SP gStaticEidosValue_Float0Point5;
extern EidosValue_Float_SP gStaticEidosValue_Float1;
extern EidosValue_Float_SP gStaticEidosValue_Float10;
extern EidosValue_Float_SP gStaticEidosValue_FloatINF;
extern EidosValue_Float_SP gStaticEidosValue_FloatNAN;
extern EidosValue_Float_SP gStaticEidosValue_FloatE;
extern EidosValue_Float_SP gStaticEidosValue_FloatPI;

extern EidosValue_String_SP gStaticEidosValue_StringEmpty;
extern EidosValue_String_SP gStaticEidosValue_StringSpace;
extern EidosValue_String_SP gStaticEidosValue_StringAsterisk;
extern EidosValue_String_SP gStaticEidosValue_StringDoubleAsterisk;


// EidosValueType is an enum of the possible types for EidosValue objects.  Note that all of these types are vectors of the stated
// type; all objects in Eidos are vectors.  The order of these types is in type-promotion order, from lowest to highest, except
// that NULL never gets promoted to any other type, and nothing ever gets promoted to object type.
enum class EidosValueType : uint8_t
{
	kValueVOID = 0,		// special void type; this cannot be mixed with other types or promoted to other types, cannot be passed to a function/method, cannot be assigned to a variable
	kValueNULL,			// special NULL type; this cannot be mixed with other types or promoted to other types
	
	kValueLogical,		// logicals (eidos_logical_t)
	kValueInt,			// integers (int64_t)
	kValueFloat,		// floats (double)
	kValueString,		// strings (std:string)
	
	kValueObject		// a vector of EidosObjectElement objects: these represent built-in objects with properties and methods
};

std::string StringForEidosValueType(const EidosValueType p_type);
std::ostream &operator<<(std::ostream &p_outstream, const EidosValueType p_type);


// EidosValueMask is a uint32_t used as a bit mask to identify permitted types for EidosValue objects (arguments, returns)
// Note that these mask values must correspond to the values in EidosValueType directly; (1 << (int)type) == mask must be true.
typedef uint32_t EidosValueMask;

const EidosValueMask kEidosValueMaskNone =			0x00000000;
const EidosValueMask kEidosValueMaskVOID =			0x00000001;
const EidosValueMask kEidosValueMaskNULL =			0x00000002;
const EidosValueMask kEidosValueMaskLogical =		0x00000004;
const EidosValueMask kEidosValueMaskInt =			0x00000008;
const EidosValueMask kEidosValueMaskFloat =			0x00000010;
const EidosValueMask kEidosValueMaskString =		0x00000020;
const EidosValueMask kEidosValueMaskObject =		0x00000040;

const EidosValueMask kEidosValueMaskOptional =		0x80000000;
const EidosValueMask kEidosValueMaskSingleton =		0x40000000;
const EidosValueMask kEidosValueMaskFlagStrip =		0x3FFFFFFF;

const EidosValueMask kEidosValueMaskNumeric =		(kEidosValueMaskInt | kEidosValueMaskFloat);									// integer or float
const EidosValueMask kEidosValueMaskLogicalEquiv =	(kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskFloat);			// logical, integer, or float
const EidosValueMask kEidosValueMaskAnyBase =		(kEidosValueMaskNULL | kEidosValueMaskLogicalEquiv | kEidosValueMaskString);	// any non-void type except object
const EidosValueMask kEidosValueMaskAny =			(kEidosValueMaskAnyBase | kEidosValueMaskObject);								// any non-void type including object

std::string StringForEidosValueMask(const EidosValueMask p_mask, const EidosObjectClass *p_object_class, const std::string &p_name, EidosValue *p_default);
//std::ostream &operator<<(std::ostream &p_outstream, const EidosValueMask p_mask);	// can't do this since EidosValueMask is just uint32_t


// EidosTypeSpecifier is a struct that provides a full type specifier, including an optional object class, for a value
typedef struct {
	EidosValueMask type_mask;					// can specify kEidosValueMaskNone for no known type, or any combination of type masks
	const EidosObjectClass *object_class;		// if kEidosValueMaskObject is included in type_mask, this can specify a class (or can be nullptr)
} EidosTypeSpecifier;


#pragma mark -
#pragma mark Comparing EidosValues
#pragma mark -

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

EidosCompareFunctionPtr Eidos_GetCompareFunctionForTypes(EidosValueType p_type1, EidosValueType p_type2, const EidosToken *p_blame_token);


#pragma mark -
#pragma mark EidosValue
#pragma mark -

//	*********************************************************************************************************
//
//	A class representing a value resulting from script evaluation.  Eidos is quite dynamically typed;
//	problems cause runtime exceptions.  EidosValue is the abstract base class for all value classes.
//

// BCH 11/27/2017: I am converting some of the EidosValue implementations that used a std::vector internally
// to use a malloc'ed pointer instead.  This buys us several concrete speed benefits:
//
//	-	When generating a new vector value, we can allocate the EidosValue object, set its size to a known N,
//		and then set the values inside it without needing to either zero-initialize the buffer first or set
//		the count after each value added.  In contrast, std::vector does not allow this mode of operation;
//		you can either resize the vector first (which zero-initializes), or you can reserve but not resize
//		(avoiding the zero-initialize) and then push_back() values, but that writes a new count value out to
//		memory on every push.  Since this sort of large-N result generation is very common, this is a big win.
//
//	-	When adding new values, it lets us skip both bounds-checking and capacity-checking when we know we
//		have allocated a sufficiently big buffer to hold the push.  This is sometimes worthwhile as well;
//		sometimes the checks done by std::vector end up taking zero time due to pipelining, but sometimes not.
//
//	-	We no longer rely on the compiler being super-smart to optimize things for us.  In practice modern
//		C++ compilers *are* super-smart, but only when their optimization level is set high.  In practice,
//		this means that we see a significant speedup compared to std::vector when running an unoptimized
//		debugging build, which is a nice benefit for me, albeit with no impact for end users.

class EidosValue
{
	//	This class has its assignment operator disabled, to prevent accidental copying.
protected:
	
	mutable uint32_t intrusive_ref_count_;					// used by Eidos_intrusive_ptr
	const EidosValueType cached_type_;						// allows Type() to be an inline function; cached at construction
	uint8_t invisible_;										// as in R; if true, the value will not normally be printed to the console
	uint8_t is_singleton_;									// allows Count() and IsSingleton() to be inline; cached at construction
	uint8_t registered_for_patching_;						// used by EidosValue_Object, otherwise UNINITIALIZED; declared here for reasons of memory packing
	
	int64_t *dim_;											// nullptr for vectors; points to a malloced, OWNED array of dimensions for matrices and arrays
															//    when allocated, the first value in the buffer is a count of the dimensions that follow
	virtual void _CopyDimensionsFromValue(const EidosValue *p_value);											// do not call directly; called by CopyDimensionsFromValue()
	void PrintMatrixFromIndex(int64_t p_ncol, int64_t p_nrow, int64_t p_start_index, std::ostream &p_ostream) const;
	
public:
	
	EidosValue(const EidosValue &p_original) = delete;		// no copy-construct
	EidosValue& operator=(const EidosValue&) = delete;		// no copying
	
	EidosValue(void) = delete;									// no null constructor
	EidosValue(EidosValueType p_value_type, bool p_singleton);	// must construct with a type identifier and singleton flag, which will be cached
	virtual ~EidosValue(void);
	
	// basic methods
	inline __attribute__((always_inline)) EidosValueType Type(void) const { return cached_type_; }	// the type of the vector, cached at construction
	inline __attribute__((always_inline)) bool IsSingleton(void) const { return is_singleton_; }	// true is the subclass is a singleton subclass (not just if Count()==1)
	inline __attribute__((always_inline)) int Count(void) const { return (is_singleton_ ? 1 : Count_Virtual()); }	// avoid the virtual function call for singletons
	
	virtual const std::string &ElementType(void) const = 0;	// the type of the elements contained by the vector
	virtual int Count_Virtual(void) const = 0;				// the number of values in the vector
	void Print(std::ostream &p_ostream) const;				// standard printing; same as operator<<
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const = 0;
	
	// object invisibility; note invisibility should only be changed on uniquely owned objects, to avoid side effects
	inline __attribute__((always_inline)) bool Invisible(void) const							{ return invisible_; }
	inline __attribute__((always_inline)) void SetInvisible(bool p_invisible)					{ invisible_ = p_invisible; }
	
	// basic subscript access; abstract here since we want to force subclasses to define this
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token) = 0;
	
	// fetching individual values; these convert type if necessary, and (base class behavior) raise if impossible
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	virtual EidosObjectElement *ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const = 0;
	
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
	void RaiseForUnimplementedVectorCall(void) const __attribute__((__noreturn__)) __attribute__((analyzer_noreturn));
	void RaiseForUnsupportedConversionCall(const EidosToken *p_blame_token) const __attribute__((__noreturn__)) __attribute__((analyzer_noreturn));
	void RaiseForCapacityViolation(void) const __attribute__((__noreturn__)) __attribute__((analyzer_noreturn));
	void RaiseForRangeViolation(void) const __attribute__((__noreturn__)) __attribute__((analyzer_noreturn));
	
	virtual const EidosValue_Logical *LogicalVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual EidosValue_Logical *LogicalVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const std::vector<std::string> *StringVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual std::vector<std::string> *StringVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const EidosValue_Int_vector *IntVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual EidosValue_Int_vector *IntVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const EidosValue_Float_vector *FloatVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual EidosValue_Float_vector *FloatVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual const EidosValue_Object_vector *ObjectElementVector(void) const { RaiseForUnimplementedVectorCall(); }
	virtual EidosValue_Object_vector *ObjectElementVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	
	// Dimension support, for matrices and arrays
	inline __attribute__((always_inline)) bool IsArray(void) const { return !!dim_; }							// true if we have a dimensions buffer – any array, including a matrix
	inline __attribute__((always_inline)) int DimensionCount(void) const { return (!dim_) ? 1 : (int)*dim_; }	// 1 for vectors, 2 for matrices, 2...n for arrays (1 not allowed for arrays)
	inline __attribute__((always_inline)) const int64_t *Dimensions(void) const { return (!dim_) ? nullptr : dim_ + 1; }	// nullptr or a pointer into the dim_ buffer
	
	inline __attribute__((always_inline)) EidosValue *CopyDimensionsFromValue(const EidosValue *p_value)		// copy dimensions from another value; checks for validity, can raise
	{
		if ((p_value && p_value->dim_) || dim_)
			_CopyDimensionsFromValue(p_value);
		return this;
	}
	
	void SetDimensions(int64_t p_dim_count, const int64_t *p_dim_buffer);										// can be 1/nullptr to make the value a vector; 0 is not allowed
	
	static bool MatchingDimensions(const EidosValue *p_value1, const EidosValue *p_value2);						// true if dimensionalities are identical; works for vectors as well as matrices/arrays
	static inline __attribute__((always_inline)) EidosValue *BinaryOperationDimensionSource(EidosValue *p_value1, EidosValue *p_value2)	// chooses which value's dimensionality will carry through a binary op
	{
		int dim1 = p_value1->DimensionCount();
		int dim2 = p_value2->DimensionCount();
		
		if ((dim1 == 1) && (dim2 == 1))
			return nullptr;							// neither is a matrix/array, so it doesn't matter; avoid the EidosValue_SP overhead
		
		int count1 = p_value1->Count();
		int count2 = p_value2->Count();
		
		if (dim2 == 1)
		{
			// p_value1 is a matrix/array, p_value2 is not; prefer the matrix, except that we won't promote a non-singleton vector to be a matrix
			if ((count1 == 1) && (count2 != 1))
				return p_value2;
			return p_value1;
		}
		else if (dim1 == 1)
		{
			// p_value2 is a matrix/array, p_value1 is not; prefer the matrix, except that we won't promote a non-singleton vector to be a matrix
			if ((count2 == 1) && (count1 != 1))
				return p_value1;
			return p_value2;
		}
		else
		{
			return p_value1;							// both are a matrix/array, so it doesn't matter (either they are conformable, or there's an error anyway)
		}
	}
	
	EidosValue_SP Subset(std::vector<std::vector<int64_t>> &p_inclusion_indices, bool p_drop, const EidosToken *p_blame_token);
	
	// Eidos_intrusive_ptr support; we use Eidos_intrusive_ptr as a fast smart pointer to EidosValue.
	inline __attribute__((always_inline)) uint32_t UseCount() const { return intrusive_ref_count_; }
	inline __attribute__((always_inline)) void StackAllocated() { intrusive_ref_count_++; }			// used with stack-allocated EidosValues that have to be put under Eidos_intrusive_ptr
	
	friend void Eidos_intrusive_ptr_add_ref(const EidosValue *p_value);
	friend void Eidos_intrusive_ptr_release(const EidosValue *p_value);
	
	// This is a hack scheme to track EidosValue allocations and deallocations, as a way to help debug leaks
	// To enable it, change the #undef to #define.  When you run Eidos tests, a summary of persistent EidosValues will print.
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
	++(p_value->intrusive_ref_count_);
}

inline __attribute__((always_inline)) void Eidos_intrusive_ptr_release(const EidosValue *p_value)
{
	if ((--(p_value->intrusive_ref_count_)) == 0)
	{
		// We no longer delete; all EidosValues under Eidos_intrusive_ptr should have been allocated out of gEidosValuePool, so it handles the free
		//delete p_value;
		p_value->~EidosValue();
		gEidosValuePool->DisposeChunk(const_cast<EidosValue*>(p_value));
	}
}


#pragma mark -
#pragma mark EidosValue_VOID
#pragma mark -

//	*********************************************************************************************************
//
//	EidosValue_VOID represents void values in Eidos.  We have one static global EidosValue_VOID instance,
//	which should be used for all void values.
//

class EidosValue_VOID : public EidosValue
{
public:
	EidosValue_VOID(const EidosValue_VOID &p_original) = delete;	// no copy-construct
	EidosValue_VOID& operator=(const EidosValue_VOID&) = delete;	// no copying
	
	inline EidosValue_VOID(void) : EidosValue(EidosValueType::kValueVOID, false) { }
	inline virtual ~EidosValue_VOID(void) {}
	
	static EidosValue_VOID_SP Static_EidosValue_VOID(void);
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const;
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual eidos_logical_t LogicalAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual std::string StringAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual int64_t IntAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual double FloatAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


#pragma mark -
#pragma mark EidosValue_NULL
#pragma mark -

//	*********************************************************************************************************
//
//	EidosValue_NULL represents NULL values in Eidos.  We have two static global EidosValue_NULL instances,
//	representing invisible versus non-invisible NULL.
//

class EidosValue_NULL : public EidosValue
{
public:
	EidosValue_NULL(const EidosValue_NULL &p_original) = delete;	// no copy-construct
	EidosValue_NULL& operator=(const EidosValue_NULL&) = delete;	// no copying
	
	inline EidosValue_NULL(void) : EidosValue(EidosValueType::kValueNULL, false) { }
	inline virtual ~EidosValue_NULL(void) {}
	
	static EidosValue_NULL_SP Static_EidosValue_NULL(void);
	static EidosValue_NULL_SP Static_EidosValue_NULL_Invisible(void);
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const;
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);

	virtual eidos_logical_t LogicalAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual std::string StringAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual int64_t IntAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual double FloatAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


#pragma mark -
#pragma mark EidosValue_Logical
#pragma mark -

//	*********************************************************************************************************
//
//	EidosValue_Logical represents logical (bool) values in Eidos.  Unlike other EidosValue types, for
//	logical the EidosValue_Logical class itself is a non-abstract class that models a vector of logical
//	values; there is no singleton version.  There is a subclass, EidosValue_Logical_const, that is used
//	by the two shared T and F EidosValues, gStaticEidosValue_LogicalT and gStaticEidosValue_LogicalF;
//	it tries to enforce constness on those globals by making the virtual function LogicalVector_Mutable(),
//	which is used to go from an EidosValue* to a EidosValue_Logical*, raise, and by making other virtual
//	functions that would modify the value raise as well.  This is not entirely bullet-proof, since one
//	could cast to EidosValue_Logical instead of using LogicalVector_Mutable(), but you're not supposed
//	to do that.  EidosValue_Logical_const pretends to be a singleton class, by setting is_singleton_,
//	but that is probably non-essential.
//

class EidosValue_Logical : public EidosValue
{
protected:
	eidos_logical_t *values_ = nullptr;
	size_t count_ = 0, capacity_ = 0;
	
protected:
	explicit EidosValue_Logical(eidos_logical_t p_logical1);		// protected to encourage use of EidosValue_Logical_const for this
	
public:
	EidosValue_Logical(const EidosValue_Logical &p_original) = delete;	// no copy-construct
	EidosValue_Logical& operator=(const EidosValue_Logical&) = delete;	// no copying
	
	inline EidosValue_Logical(void) : EidosValue(EidosValueType::kValueLogical, false) { }
	explicit EidosValue_Logical(const std::vector<eidos_logical_t> &p_logicalvec);
	explicit EidosValue_Logical(std::initializer_list<eidos_logical_t> p_init_list);
	explicit EidosValue_Logical(const eidos_logical_t *p_values, size_t p_count);
	inline virtual ~EidosValue_Logical(void) { free(values_); }
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const;
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const;
	
	virtual const EidosValue_Logical *LogicalVector(void) const { return this; }
	virtual EidosValue_Logical *LogicalVector_Mutable(void) { return this; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual EidosValue_SP NewMatchingType(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
	
	// vector lookalike methods; not virtual, only for clients with a EidosValue_Logical*
	EidosValue_Logical *reserve(size_t p_reserved_size);				// as in std::vector
	EidosValue_Logical *resize_no_initialize(size_t p_new_size);		// does not zero-initialize, unlike std::vector!
	void expand(void);													// expand to fit (at least) one new value
	void erase_index(size_t p_index);									// a weak substitute for erase()
	
	inline __attribute__((always_inline)) eidos_logical_t *data(void) { return values_; }
	inline __attribute__((always_inline)) const eidos_logical_t *data(void) const { return values_; }
	inline __attribute__((always_inline)) size_t size(void) const { return count_; }
	inline __attribute__((always_inline)) void push_logical(eidos_logical_t p_logical)
	{
		if (count_ == capacity_) expand();
		values_[count_++] = p_logical;
	}
	inline __attribute__((always_inline)) void push_logical_no_check(eidos_logical_t p_logical) {
#if DEBUG
		// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
		if (count_ == capacity_) RaiseForCapacityViolation();
#endif
		values_[count_++] = p_logical;
	}
	inline __attribute__((always_inline)) void set_logical_no_check(eidos_logical_t p_logical, size_t p_index) {
#if DEBUG
		// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
		if (p_index >= count_) RaiseForRangeViolation();
#endif
		values_[p_index] = p_logical;
	}
};

class EidosValue_Logical_const : public EidosValue_Logical
{
protected:
	virtual void _CopyDimensionsFromValue(const EidosValue *p_value);
public:
	EidosValue_Logical_const(const EidosValue_Logical_const &p_original) = delete;	// no copy-construct
	EidosValue_Logical_const(void) = delete;										// no default constructor
	EidosValue_Logical_const& operator=(const EidosValue_Logical_const&) = delete;	// no copying
	explicit EidosValue_Logical_const(eidos_logical_t p_logical1);
	virtual ~EidosValue_Logical_const(void);											// destructor calls EidosTerminate()
	
	static EidosValue_Logical_SP Static_EidosValue_Logical_T(void);
	static EidosValue_Logical_SP Static_EidosValue_Logical_F(void);
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because this subclass represents only truly immutable objects
	virtual EidosValue_Logical *LogicalVector_Mutable(void) { RaiseForUnimplementedVectorCall(); }
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


#pragma mark -
#pragma mark EidosValue_String
#pragma mark -

//	*********************************************************************************************************
//
//	EidosValue_String represents string (C++ std::string) values in Eidos.  The subclass
//	EidosValue_String_vector is the standard instance class, used to hold vectors of strings.
//	EidosValue_String_singleton is used for speed, to represent single values.
//

class EidosValue_String : public EidosValue
{
protected:
	explicit inline EidosValue_String(bool p_singleton) : EidosValue(EidosValueType::kValueString, p_singleton) {}
	
public:
	EidosValue_String(const EidosValue_String &p_original) = delete;		// no copy-construct
	EidosValue_String(void) = delete;										// no default constructor
	EidosValue_String& operator=(const EidosValue_String&) = delete;		// no copying
	inline virtual ~EidosValue_String(void) { }
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const = 0;
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const;
	
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
	// this is not converted to a malloced buffer because unlike the other types, we can't get away with
	// not initializing the memory belonging to a std::string, so the malloc strategy doesn't work
	std::vector<std::string> values_;
	
public:
	EidosValue_String_vector(const EidosValue_String_vector &p_original) = delete;	// no copy-construct
	EidosValue_String_vector& operator=(const EidosValue_String_vector&) = delete;	// no copying
	
	inline EidosValue_String_vector(void) : EidosValue_String(false) { }
	explicit EidosValue_String_vector(const std::vector<std::string> &p_stringvec);
	EidosValue_String_vector(double *p_doublebuf, int p_buffer_length);
	//explicit EidosValue_String_vector(const std::string &p_string1);		// disabled to encourage use of EidosValue_String_singleton for this case
	explicit EidosValue_String_vector(std::initializer_list<const std::string> p_init_list);
	inline virtual ~EidosValue_String_vector(void) { }
	
	virtual int Count_Virtual(void) const;
	
	virtual const std::vector<std::string> *StringVector(void) const { return &values_; }
	virtual std::vector<std::string> *StringVector_Mutable(void) { return &values_; }
	inline __attribute__((always_inline)) void PushString(const std::string &p_string) { values_.emplace_back(p_string); }
	inline __attribute__((always_inline)) EidosValue_String_vector *Reserve(int p_reserved_size) { values_.reserve(p_reserved_size); return this; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
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
	EidosScript *cached_script_ = nullptr;	// cached by executeLambda(), apply(), and sapply() to avoid multiple tokenize/parse overhead
	
public:
	EidosValue_String_singleton(const EidosValue_String_singleton &p_original) = delete;	// no copy-construct
	EidosValue_String_singleton& operator=(const EidosValue_String_singleton&) = delete;	// no copying
	EidosValue_String_singleton(void) = delete;
	explicit inline EidosValue_String_singleton(const std::string &p_string1) : value_(p_string1), EidosValue_String(true) { }
	inline virtual ~EidosValue_String_singleton(void) { delete cached_script_; }
	
	virtual int Count_Virtual(void) const;
	
	inline __attribute__((always_inline)) const std::string &StringValue(void) const { return value_; }
	inline __attribute__((always_inline)) std::string &StringValue_Mutable(void) { delete cached_script_; cached_script_ = nullptr; return value_; }			// very dangerous; do not use
	inline __attribute__((always_inline)) void SetValue(const std::string &p_string) { delete cached_script_; cached_script_ = nullptr; value_ = p_string; }	// very dangerous; used only in Evaluate_For()
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
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


#pragma mark -
#pragma mark EidosValue_Int
#pragma mark -

//	*********************************************************************************************************
//
//	EidosValue_Int represents integer (C++ int64_t) values in Eidos.  The subclass
//	EidosValue_Int_vector is the standard instance class, used to hold vectors of floats.
//	EidosValue_Int_singleton is used for speed, to represent single values.
//

class EidosValue_Int : public EidosValue
{
protected:
	explicit inline EidosValue_Int(bool p_singleton) : EidosValue(EidosValueType::kValueInt, p_singleton) {}
	
public:
	EidosValue_Int(const EidosValue_Int &p_original) = delete;			// no copy-construct
	EidosValue_Int(void) = delete;										// no default constructor
	EidosValue_Int& operator=(const EidosValue_Int&) = delete;			// no copying
	inline virtual ~EidosValue_Int(void) { }
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const = 0;
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const;
	
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
	int64_t *values_ = nullptr;
	size_t count_ = 0, capacity_ = 0;
	
public:
	EidosValue_Int_vector(const EidosValue_Int_vector &p_original) = delete;	// no copy-construct
	EidosValue_Int_vector& operator=(const EidosValue_Int_vector&) = delete;	// no copying
	
	inline EidosValue_Int_vector(void) : EidosValue_Int(false) { }
	explicit EidosValue_Int_vector(const std::vector<int16_t> &p_intvec);
	explicit EidosValue_Int_vector(const std::vector<int32_t> &p_intvec);
	explicit EidosValue_Int_vector(const std::vector<int64_t> &p_intvec);
	//explicit EidosValue_Int_vector(int64_t p_int1);		// disabled to encourage use of EidosValue_Int_singleton for this case
	explicit EidosValue_Int_vector(std::initializer_list<int64_t> p_init_list);
	explicit EidosValue_Int_vector(const int64_t *p_values, size_t p_count);
	inline virtual ~EidosValue_Int_vector(void) { free(values_); }
	
	virtual int Count_Virtual(void) const;
	
	virtual const EidosValue_Int_vector *IntVector(void) const { return this; }
	virtual EidosValue_Int_vector *IntVector_Mutable(void) { return this; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
	
	// vector lookalike methods; not virtual, only for clients with a EidosValue_Int_vector*
	EidosValue_Int_vector *reserve(size_t p_reserved_size);				// as in std::vector
	EidosValue_Int_vector *resize_no_initialize(size_t p_new_size);		// does not zero-initialize, unlike std::vector!
	void expand(void);													// expand to fit (at least) one new value
	void erase_index(size_t p_index);									// a weak substitute for erase()
	
	inline __attribute__((always_inline)) int64_t *data(void) { return values_; }
	inline __attribute__((always_inline)) const int64_t *data(void) const { return values_; }
	inline __attribute__((always_inline)) size_t size(void) const { return count_; }
	inline __attribute__((always_inline)) void push_int(int64_t p_int)
	{
		if (count_ == capacity_) expand();
		values_[count_++] = p_int;
	}
	inline __attribute__((always_inline)) void push_int_no_check(int64_t p_int) {
#if DEBUG
		// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
		if (count_ == capacity_) RaiseForCapacityViolation();
#endif
		values_[count_++] = p_int;
	}
	inline __attribute__((always_inline)) void set_int_no_check(int64_t p_int, size_t p_index) {
#if DEBUG
		// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
		if (p_index >= count_) RaiseForRangeViolation();
#endif
		values_[p_index] = p_int;
	}
};

class EidosValue_Int_singleton : public EidosValue_Int
{
protected:
	int64_t value_;
	
public:
	EidosValue_Int_singleton(const EidosValue_Int_singleton &p_original) = delete;	// no copy-construct
	EidosValue_Int_singleton& operator=(const EidosValue_Int_singleton&) = delete;	// no copying
	EidosValue_Int_singleton(void) = delete;
	explicit inline EidosValue_Int_singleton(int64_t p_int1) : value_(p_int1), EidosValue_Int(true) { }
	inline virtual ~EidosValue_Int_singleton(void) { }
	
	virtual int Count_Virtual(void) const;
	
	inline __attribute__((always_inline)) int64_t IntValue(void) const { return value_; }
	inline __attribute__((always_inline)) int64_t &IntValue_Mutable(void) { return value_; }	// very dangerous; used only in Evaluate_Assign()
	inline __attribute__((always_inline)) void SetValue(int64_t p_int) { value_ = p_int; }		// very dangerous; used only in Evaluate_For()
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosValue_SP CopyValues(void) const;
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because there is no backing vector
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


#pragma mark -
#pragma mark EidosValue_Float
#pragma mark -

//	*********************************************************************************************************
//
//	EidosValue_Float represents floating-point (C++ double) values in Eidos.  The subclass
//	EidosValue_Float_vector is the standard instance class, used to hold vectors of floats.
//	EidosValue_Float_singleton is used for speed, to represent single values.
//

class EidosValue_Float : public EidosValue
{
protected:
	explicit inline EidosValue_Float(bool p_singleton) : EidosValue(EidosValueType::kValueFloat, p_singleton) {}
	
public:
	EidosValue_Float(const EidosValue_Float &p_original) = delete;			// no copy-construct
	EidosValue_Float(void) = delete;										// no default constructor
	EidosValue_Float& operator=(const EidosValue_Float&) = delete;			// no copying
	inline virtual ~EidosValue_Float(void) { }
	
	virtual const std::string &ElementType(void) const;
	virtual int Count_Virtual(void) const = 0;
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const;
	
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
	double *values_ = nullptr;
	size_t count_ = 0, capacity_ = 0;
	
public:
	EidosValue_Float_vector(const EidosValue_Float_vector &p_original) = delete;	// no copy-construct
	EidosValue_Float_vector& operator=(const EidosValue_Float_vector&) = delete;	// no copying
	
	inline EidosValue_Float_vector(void) : EidosValue_Float(false) { }
	explicit EidosValue_Float_vector(const std::vector<double> &p_doublevec);
	//explicit EidosValue_Float_vector(double p_float1);		// disabled to encourage use of EidosValue_Float_singleton for this case
	explicit EidosValue_Float_vector(std::initializer_list<double> p_init_list);
	explicit EidosValue_Float_vector(const double *p_values, size_t p_count);
	inline virtual ~EidosValue_Float_vector(void) { free(values_); }
	
	virtual int Count_Virtual(void) const;
	
	virtual const EidosValue_Float_vector *FloatVector(void) const { return this; }
	virtual EidosValue_Float_vector *FloatVector_Mutable(void) { return this; }
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
	
	// vector lookalike methods; not virtual, only for clients with a EidosValue_Int_vector*
	EidosValue_Float_vector *reserve(size_t p_reserved_size);			// as in std::vector
	EidosValue_Float_vector *resize_no_initialize(size_t p_new_size);	// does not zero-initialize, unlike std::vector!
	void expand(void);													// expand to fit (at least) one new value
	void erase_index(size_t p_index);									// a weak substitute for erase()
	
	inline __attribute__((always_inline)) double *data(void) { return values_; }
	inline __attribute__((always_inline)) const double *data(void) const { return values_; }
	inline __attribute__((always_inline)) size_t size(void) const { return count_; }
	inline __attribute__((always_inline)) void push_float(double p_float)
	{
		if (count_ == capacity_) expand();
		values_[count_++] = p_float;
	}
	inline __attribute__((always_inline)) void push_float_no_check(double p_float) {
#if DEBUG
		// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
		if (count_ == capacity_) RaiseForCapacityViolation();
#endif
		values_[count_++] = p_float;
	}
	inline __attribute__((always_inline)) void set_float_no_check(double p_float, size_t p_index) {
#if DEBUG
		// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
		if (p_index >= count_) RaiseForRangeViolation();
#endif
		values_[p_index] = p_float;
	}
};

class EidosValue_Float_singleton : public EidosValue_Float
{
protected:
	double value_;
	
public:
	EidosValue_Float_singleton(const EidosValue_Float_singleton &p_original) = delete;	// no copy-construct
	EidosValue_Float_singleton& operator=(const EidosValue_Float_singleton&) = delete;	// no copying
	EidosValue_Float_singleton(void) = delete;
	explicit inline EidosValue_Float_singleton(double p_float1) : value_(p_float1), EidosValue_Float(true) { }
	inline virtual ~EidosValue_Float_singleton(void) { }
	
	virtual int Count_Virtual(void) const;
	
	inline __attribute__((always_inline)) double FloatValue(void) const { return value_; }
	inline __attribute__((always_inline)) double &FloatValue_Mutable(void) { return value_; }	// very dangerous; used only in Evaluate_Assign()
	inline __attribute__((always_inline)) void SetValue(double p_float) { value_ = p_float; }	// very dangerous; used only in Evaluate_For()
	
	virtual eidos_logical_t LogicalAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual std::string StringAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual int64_t IntAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual double FloatAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosObjectElement *ObjectElementAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual EidosValue_SP CopyValues(void) const;
	
	virtual EidosValue_SP VectorBasedCopy(void) const;
	
	// prohibited actions because there is no backing vector
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	virtual void Sort(bool p_ascending);
};


#pragma mark -
#pragma mark EidosValue_Object
#pragma mark -

//	*********************************************************************************************************
//
//	EidosValue_Object represents objects in Eidos: entities that have properties and can respond to
//	methods.  The subclass EidosValue_Object_vector is the standard instance class, used to hold vectors
//	of objects.  EidosValue_Object_singleton is used for speed, to represent single values.
//

// EidosObjectElement supports a retain/release mechanism that disposes of objects when no longer
// referenced, which client code can get by subclassing EidosObjectElementInternal instead of
// EidosObjectElement and overriding NeedsRetainRelease(void) in their EidosObjectClass subclass
// to return true.  However, SLiM manages its own object lifetimes, so the only EidosObjectElement
// subclass that actually uses this facility is EidosTestElement, which is used only in test code.
// Supporting retain/release involves a bit of extra checking with every add/remove of an object
// element, so by default the support is disabled, and EidosTestElement simply leaks.  Define this
// preprocessor flag to turn the retain/release mechanism on.
#undef EIDOS_OBJECT_RETAIN_RELEASE

class EidosValue_Object : public EidosValue
{
protected:
	const EidosObjectClass *class_;		// can be gEidos_UndefinedClassObject if the vector is empty
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	bool class_needs_retain_release_;	// cached from NeedsRetainRelease() of class_; true until class_ is set, to catch errors
#endif
	
	EidosValue_Object(bool p_singleton, const EidosObjectClass *p_class);
	EidosValue_Object(bool p_singleton, const EidosObjectClass *p_class, bool p_register_for_patching);		// a variant for self-pointer EidosValues; not for general use
	
public:
	EidosValue_Object(const EidosValue_Object &p_original) = delete;				// no copy-construct
	EidosValue_Object(void) = delete;												// no default constructor
	EidosValue_Object& operator=(const EidosValue_Object&) = delete;				// no copying
	virtual ~EidosValue_Object(void);
	
	void RaiseForClassMismatch(void) const;
	void DeclareClassFromElement(const EidosObjectElement *p_element, bool p_undeclared_is_error = false);
	
	virtual const std::string &ElementType(void) const;
	inline __attribute__((always_inline)) const EidosObjectClass *Class(void) const { return class_; }
	virtual int Count_Virtual(void) const = 0;
	virtual void PrintValueAtIndex(const int p_idx, std::ostream &p_ostream) const;
	
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
	
	virtual EidosValue_SP ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosInstanceMethodSignature *p_call_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) = 0;
	
	// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
	virtual void PatchPointersByAdding(std::uintptr_t p_pointer_difference) = 0;
	virtual void PatchPointersBySubtracting(std::uintptr_t p_pointer_difference) = 0;
};

class EidosValue_Object_vector : public EidosValue_Object
{
protected:
	EidosObjectElement **values_ = nullptr;		// these may use a retain/release system of ownership; see below
	size_t count_ = 0, capacity_ = 0;
	
public:
	EidosValue_Object_vector(const EidosValue_Object_vector &p_original);				// can copy-construct
	EidosValue_Object_vector& operator=(const EidosValue_Object_vector&) = delete;		// no copying
	
	explicit inline EidosValue_Object_vector(const EidosObjectClass *p_class) : EidosValue_Object(false, p_class) { }		// can be gEidos_UndefinedClassObject
	explicit EidosValue_Object_vector(const std::vector<EidosObjectElement *> &p_elementvec, const EidosObjectClass *p_class);
	//explicit EidosValue_Object_vector(EidosObjectElement *p_element1);		// disabled to encourage use of EidosValue_Object_singleton for this case
	explicit EidosValue_Object_vector(std::initializer_list<EidosObjectElement *> p_init_list, const EidosObjectClass *p_class);
	explicit EidosValue_Object_vector(EidosObjectElement **p_values, size_t p_count, const EidosObjectClass *p_class);
	virtual ~EidosValue_Object_vector(void);
	
	virtual int Count_Virtual(void) const;
	
	virtual eidos_logical_t LogicalAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual std::string StringAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual int64_t IntAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual double FloatAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual EidosObjectElement *ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	virtual const EidosValue_Object_vector *ObjectElementVector(void) const { return this; }
	virtual EidosValue_Object_vector *ObjectElementVector_Mutable(void) { return this; }
	
	virtual EidosValue_SP GetValueAtIndex(const int p_idx, const EidosToken *p_blame_token) const;
	virtual void SetValueAtIndex(const int p_idx, const EidosValue &p_value, const EidosToken *p_blame_token);
	
	virtual EidosValue_SP CopyValues(void) const;
	virtual void PushValueFromIndexOfEidosValue(int p_idx, const EidosValue &p_source_script_value, const EidosToken *p_blame_token);
	void SortBy(const std::string &p_property, bool p_ascending);
	
	// Property and method support; defined only on EidosValue_Object, not EidosValue.  The methods that a
	// EidosValue_Object instance defines depend upon the type of the EidosObjectElement objects it contains.
	virtual EidosValue_SP GetPropertyOfElements(EidosGlobalStringID p_property_id) const;
	virtual void SetPropertyOfElements(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosInstanceMethodSignature *p_call_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
	virtual void PatchPointersByAdding(std::uintptr_t p_pointer_difference);
	virtual void PatchPointersBySubtracting(std::uintptr_t p_pointer_difference);
	
	// vector lookalike methods; not virtual, only for clients with a EidosValue_Int_vector*
	EidosValue_Object_vector *reserve(size_t p_reserved_size);			// as in std::vector
	EidosValue_Object_vector *resize_no_initialize(size_t p_new_size);	// does not zero-initialize, unlike std::vector!
	void expand(void);													// expand to fit (at least) one new value
	void erase_index(size_t p_index);									// a weak substitute for erase()
	
	inline __attribute__((always_inline)) EidosObjectElement **data(void) { return values_; }		// the accessors below should be used to modify, since they handle Retain()/Release()
	inline __attribute__((always_inline)) EidosObjectElement * const *data(void) const { return values_; }
	inline __attribute__((always_inline)) size_t size(void) const { return count_; }
	void push_object_element(EidosObjectElement *p_object);
	void push_object_element_no_check(EidosObjectElement *p_object);
	void set_object_element_no_check(EidosObjectElement *p_object, size_t p_index);
};

class EidosValue_Object_singleton : public EidosValue_Object
{
protected:
	EidosObjectElement *value_;		// these may use a retain/release system of ownership; see below
	
public:
	EidosValue_Object_singleton(const EidosValue_Object_singleton &p_original) = delete;		// no copy-construct
	EidosValue_Object_singleton& operator=(const EidosValue_Object_singleton&) = delete;		// no copying
	EidosValue_Object_singleton(void) = delete;
	explicit EidosValue_Object_singleton(EidosObjectElement *p_element1, const EidosObjectClass *p_class);
	explicit EidosValue_Object_singleton(EidosObjectElement *p_element1, const EidosObjectClass *p_class, bool p_register_for_patching);	// a variant for self-pointer EidosValues; not for general use
	virtual ~EidosValue_Object_singleton(void);
	
	virtual int Count_Virtual(void) const;
	
	virtual eidos_logical_t LogicalAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual std::string StringAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual int64_t IntAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual double FloatAtIndex(__attribute__((unused)) int p_idx, const EidosToken *p_blame_token) const { RaiseForUnsupportedConversionCall(p_blame_token); };
	virtual EidosObjectElement *ObjectElementAtIndex(int p_idx, const EidosToken *p_blame_token) const;
	
	inline __attribute__((always_inline)) EidosObjectElement *ObjectElementValue(void) const { return value_; }
	inline __attribute__((always_inline)) EidosObjectElement * &ObjectElementValue_Mutable(void) { return value_; }		// very dangerous; do not use
	void SetValue(EidosObjectElement *p_element);																		// very dangerous; used only in Evaluate_For()
	
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
	
	virtual EidosValue_SP ExecuteMethodCall(EidosGlobalStringID p_method_id, const EidosInstanceMethodSignature *p_call_signature, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Provided to SLiM for the Mutation-pointer hack; see EidosValue_Object::EidosValue_Object() for comments
	virtual void PatchPointersByAdding(std::uintptr_t p_pointer_difference);
	virtual void PatchPointersBySubtracting(std::uintptr_t p_pointer_difference);
};


#pragma mark -
#pragma mark EidosObjectElement
#pragma mark -

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
	
	inline EidosObjectElement(void) { }
	inline virtual ~EidosObjectElement(void) { }
	
	virtual const EidosObjectClass *Class(void) const = 0;
	
	virtual void Print(std::ostream &p_ostream) const;		// standard printing; prints ElementType()
	
	// refcounting; virtual no-ops here (for externally-owned objects), implemented in EidosObjectElementInternal
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	virtual EidosObjectElement *Retain(void);
	virtual EidosObjectElement *Release(void);
#endif
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_str(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// EidosContext is a typedef for EidosObjectElement at present, so this class is the superclass of the Context
	// object.  If that gets complicated we'll probably want to make a new EidosContext class to formalize things,
	// but for now the only addition we need for that is this virtual function stub, used for Context-defined
	// function dispatch.
	virtual EidosValue_SP ContextDefinedFunctionDispatch(const std::string &p_function_name, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};

std::ostream &operator<<(std::ostream &p_outstream, const EidosObjectElement &p_element);


// A base class for EidosObjectElement subclasses that are internal to Eidos.  See comment above.
class EidosObjectElementInternal : public EidosObjectElement
{
private:
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	uint32_t refcount_ = 1;				// start life with a refcount of 1; the allocator does not need to call Retain()
#endif
	
public:
	EidosObjectElementInternal(const EidosObjectElementInternal &p_original) = delete;		// no copy-construct
	EidosObjectElementInternal& operator=(const EidosObjectElementInternal&) = delete;		// no copying
	
	inline EidosObjectElementInternal(void) { }
	inline virtual ~EidosObjectElementInternal(void) { }
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	virtual EidosObjectElement *Retain(void);
	virtual EidosObjectElement *Release(void);
#endif
};


#pragma mark -
#pragma mark EidosObjectClass
#pragma mark -

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
protected:
	// cached dispatch tables; these are lookup tables, indexed by EidosGlobalStringID property / method ids
	bool dispatches_cached_ = false;
	
	const EidosPropertySignature *(*property_signatures_dispatch_) = nullptr;
	int32_t property_signatures_dispatch_capacity_ = 0;
	
	const EidosMethodSignature *(*method_signatures_dispatch_) = nullptr;
	int32_t method_signatures_dispatch_capacity_ = 0;
	
public:
	EidosObjectClass(const EidosObjectClass &p_original) = delete;		// no copy-construct
	EidosObjectClass& operator=(const EidosObjectClass&) = delete;		// no copying
	
	inline EidosObjectClass(void) { }
	inline virtual ~EidosObjectClass(void) { }
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	virtual bool NeedsRetainRelease(void) const;
#endif
	
	virtual const std::string &ElementType(void) const;
	
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
			return property_signatures_dispatch_[p_property_id];
		
		return nullptr;
	}
	
	inline __attribute__((always_inline)) const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const
	{
#if DEBUG
		if (!dispatches_cached_)
			RaiseForDispatchUninitialized();
#endif
		if (p_method_id < (EidosGlobalStringID)method_signatures_dispatch_capacity_)
			return method_signatures_dispatch_[p_method_id];
		
		return nullptr;
	}
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_propertySignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_methodSignature(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_size_length(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

extern EidosObjectClass *gEidos_UndefinedClassObject;


// inline methods whose implementation needs to be deferred until after the declaration of EidosObjectElement:

// check the type of a new element being added to an EidosValue_Object, and update class_needs_retain_release_
inline __attribute__((always_inline)) void EidosValue_Object::DeclareClassFromElement(const EidosObjectElement *p_element, bool p_undeclared_is_error)
{
	// no check that p_element is non-null; that should always be the case, and we don't
	// want to waste the time - a crash is fine if this invariant is not satisfied
	const EidosObjectClass *element_class = p_element->Class();
	
	if (element_class != class_)
	{
		if (!p_undeclared_is_error && (class_ == gEidos_UndefinedClassObject))
		{
			class_ = element_class;
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
			class_needs_retain_release_ = class_->NeedsRetainRelease();
#endif
		}
		else
			RaiseForClassMismatch();
	}
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element(EidosObjectElement *p_object)
{
	if (count_ == capacity_)
		expand();
	
	DeclareClassFromElement(p_object);
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
		p_object->Retain();
#endif
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element_no_check(EidosObjectElement *p_object)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (count_ == capacity_) RaiseForCapacityViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
#endif
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
		p_object->Retain();
#endif
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::set_object_element_no_check(EidosObjectElement *p_object, size_t p_index)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (p_index >= count_) RaiseForRangeViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
#endif
	EidosObjectElement *&value_slot_to_replace = values_[p_index];
	
#ifdef EIDOS_OBJECT_RETAIN_RELEASE
	if (class_needs_retain_release_)
	{
		p_object->Retain();
		if (value_slot_to_replace)
			value_slot_to_replace->Release();
	}
#endif
	
	value_slot_to_replace = p_object;
}

#endif /* defined(__Eidos__eidos_value__) */




































































