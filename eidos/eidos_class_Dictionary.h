//
//  eidos_class_Dictionary.h
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

#ifndef __Eidos__eidos_class_dictionary__
#define __Eidos__eidos_class_dictionary__


#include "eidos_globals.h"
#include "eidos_class_Object.h"
#include "json_fwd.hpp"

#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
typedef robin_hood::unordered_flat_map<std::string, EidosValue_SP> EidosDictionaryHashTable_StringKeys;
#elif STD_UNORDERED_MAP_HASHING
#include <unordered_map>
typedef std::unordered_map<std::string, EidosValue_SP> EidosDictionaryHashTable_StringKeys;
#endif

#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
typedef robin_hood::unordered_flat_map<int64_t, EidosValue_SP> EidosDictionaryHashTable_IntegerKeys;
#elif STD_UNORDERED_MAP_HASHING
#include <unordered_map>
typedef std::unordered_map<int64_t, EidosValue_SP> EidosDictionaryHashTable_IntegerKeys;
#endif


#pragma mark -
#pragma mark EidosDictionaryUnretained
#pragma mark -

extern EidosClass *gEidosDictionaryUnretained_Class;

// These are helpers for EidosDictionaryUnretained.  The purpose is to put all of its ivars into an allocated block,
// so that the overhead of inheriting from the class itself is only one pointer, unless the Dictionary functionality is
// actually used (which it usually isn't, since many SLiM objects inherit from Dictionary but rarely use it).
//
// EidosDictionary now supports keys that are either strings (the original semantics) or integers (new in SLiM 4.1).
// A given dictionary must use one or the other; the key types cannot be mixed within one dictionary, for API and
// implementation simplicity.  The keys_are_integers_ flag controls which type of key is used; this is set when this
// struct is created.  Note that code in Eidos_WarmUp() verifies that this flag is at the same memory location in both
// structs, so that we can access that flag without knowing which struct type we are using.
//
// EidosDictionary also (as of SLiM 4.1) can contain objects of any class.  It used to only allow objects that are
// under the "retain-release" memory management system, in order to guarantee that no objects were kept long-term
// unless under retain-release.  That guarantee is now satisfied, instead, by EidosDictionaryUnretained keeping
// track of whether it contains non-retain-release objects, with a flag, and a global counter keeps track of how
// many such dictionaries are extant.  That counter can be checked to throw an error if a non-retain-release object
// is kept across a "long-term boundary" (a point in the code when non-retain-release objects are freed and references
// to them would become stale).  The contains_non_retain_release_objects_ flag tracks this state.
//
// The dictionary_symbols_ hash table contains the values we are tracking.  Note that DictionarySymbols() should be
// used by all code that does not need to modify the dictionary.
//
// The dictionary_symbols_ hash table has no order for the keys.  We want to define an ordering; for Dictionary the
// ordering is sorted and computed on demand, for DataFrame it is user-defined and maintained by that class.  To allow
// DataFrame to maintain the correct ordering, call KeyAddedToDictionary_X() to register a new key when it is added,
// KeyRemovedFromDictionary_X() or AllKeysRemoved() when a key is removed, and ContentsChanged() at the end of a
// change operation (once, for either a single change or an aggregated set of changes).  The SortedKeys_X() accessors
// provide the correct sorted order for both classes, but perform a sort for Dictionary, so should be used minimally.
struct EidosDictionaryState_StringKeys
{
	uint8_t keys_are_integers_;		// 0 for EidosDictionaryState_StringKeys, 1 for EidosDictionaryState_IntegerKeys
	uint8_t contains_non_retain_release_objects_;	// 0 if none are contained, 1 if any are contained
	EidosDictionaryHashTable_StringKeys dictionary_symbols_;
};

struct EidosDictionaryState_IntegerKeys
{
	uint8_t keys_are_integers_;		// 0 for EidosDictionaryState_StringKeys, 1 for EidosDictionaryState_IntegerKeys
	uint8_t contains_non_retain_release_objects_;	// 0 if none are contained, 1 if any are contained
	EidosDictionaryHashTable_IntegerKeys dictionary_symbols_;
};

// This is a global counter of how many dictionaries exist that have their contains_non_retain_release_objects_ set.
// It can be checked at "long-term boundaries" to ensure that only retain-released objects are kept long term.
extern int64_t gEidos_DictionaryNonRetainReleaseReferenceCounter;

// This class is known in Eidos as "DictionaryBase"
class EidosDictionaryUnretained : public EidosObject
{
private:
	typedef EidosObject super;

protected:
	void *state_ptr_ = nullptr;	// pointer to EidosDictionaryState_StringKeys or EidosDictionaryState_IntegerKeys
	
	// Raise exceptions saying "keys are expected to be string, but are not string" etc.
	virtual void Raise_UsesStringKeys(void) const;
	virtual void Raise_UsesIntegerKeys(void) const;
	
	// Assert that our keys are of a given type; if not, an exception is raised; if it is undecided, neither method raises
	inline void AssertKeysAreStrings(void) const { if (!KeysAreStrings()) Raise_UsesIntegerKeys(); }
	inline void AssertKeysAreIntegers(void) const { if (!KeysAreIntegers()) Raise_UsesStringKeys(); }
	
public:
	EidosDictionaryUnretained(const EidosDictionaryUnretained &p_original) = delete;				// no copy-construct
	EidosDictionaryUnretained& operator= (const EidosDictionaryUnretained &p_original) = delete;	// no assignment
	inline EidosDictionaryUnretained(void) { }
	
	virtual ~EidosDictionaryUnretained(void) override;
	
	// Test which type our keys are; if that is undecided, both methods return true
	virtual bool KeysAreStrings(void) const { return (!state_ptr_ || !((EidosDictionaryState_StringKeys *)state_ptr_)->keys_are_integers_); }
	virtual bool KeysAreIntegers(void) const { return (!state_ptr_ || ((EidosDictionaryState_IntegerKeys *)state_ptr_)->keys_are_integers_); }
	
	// Whenever possible, access should go through these accessors to control modification of our symbols
	const EidosDictionaryHashTable_StringKeys *DictionarySymbols_StringKeys(void) const { AssertKeysAreStrings(); return state_ptr_ ? &(((EidosDictionaryState_StringKeys *)state_ptr_)->dictionary_symbols_) : nullptr; }
	const EidosDictionaryHashTable_IntegerKeys *DictionarySymbols_IntegerKeys(void) const { AssertKeysAreIntegers(); return state_ptr_ ? &(((EidosDictionaryState_IntegerKeys *)state_ptr_)->dictionary_symbols_) : nullptr; }
	
	// Provides the keys in the user-visible order: sorted for Dictionary, user-defined for DataFrame
	virtual std::vector<std::string> SortedKeys_StringKeys(void) const;
	virtual std::vector<int64_t> SortedKeys_IntegerKeys(void) const;
	
	// These methods must always be called when a key is added or removed, to allow subclasses like DataFrame to do additional work
	// Subclassers should call super; the base class may have essential behavior
	virtual void KeyAddedToDictionary_StringKeys(const std::string &p_key);
	virtual void KeyAddedToDictionary_IntegerKeys(int64_t p_key);
	virtual void KeyRemovedFromDictionary_StringKeys(const std::string &p_key);
	virtual void KeyRemovedFromDictionary_IntegerKeys(int64_t p_key);
	virtual void AllKeysRemoved(void);
	
	// This method must be called at the end of any code that changes the contents of the dictionary; it checks several invariants
	// Low-level accessors (RemoveAllKeys(), SetKeyValue(), etc.) should *not* call this; the top-level code controlling the change should
	// Subclassers should call super; the base class may have essential behavior
	virtual void ContentsChanged(const std::string &p_operation_name);
	
	int KeyCount(void) const
	{
		if (!state_ptr_)
			return 0;
		
		if (KeysAreStrings())
		{
			const EidosDictionaryHashTable_StringKeys &string_keys = ((EidosDictionaryState_StringKeys *)state_ptr_)->dictionary_symbols_;
			
			return (int)string_keys.size();
		}
		else
		{
			const EidosDictionaryHashTable_IntegerKeys &integer_keys = ((EidosDictionaryState_IntegerKeys *)state_ptr_)->dictionary_symbols_;
			
			return (int)integer_keys.size();
		}
	}
	
	virtual EidosValue_SP AllKeys(void) const;
	
	std::string Serialization_SLiM(void) const;
	EidosValue_SP Serialization_CSV(const std::string &p_delimiter) const;
	virtual nlohmann::json JSONRepresentation(void) const override;
	
	// Non-const methods: callers of these methods must ensure that ContentsChanged() is called!
	inline __attribute__((always_inline)) void RemoveAllKeys(void)
	{
		if (state_ptr_)
		{
			// We keep state_ptr_ allocated to try to avoid allocation thrash
			if (KeysAreStrings())
			{
				EidosDictionaryState_StringKeys *state_ptr = ((EidosDictionaryState_StringKeys *)state_ptr_);
				
				state_ptr->dictionary_symbols_.clear();
				AllKeysRemoved();
			}
			else
			{
				EidosDictionaryState_IntegerKeys *state_ptr = ((EidosDictionaryState_IntegerKeys *)state_ptr_);
				
				state_ptr->dictionary_symbols_.clear();
				AllKeysRemoved();
			}
		}
	}
	
	void SetKeyValue_StringKeys(const std::string &key, EidosValue_SP value);
	void SetKeyValue_IntegerKeys(int64_t key, EidosValue_SP value);
	EidosValue_SP GetValueForKey_StringKeys(const std::string &key);
	EidosValue_SP GetValueForKey_IntegerKeys(int64_t key);
	
	void AddKeysAndValuesFrom(EidosDictionaryUnretained *p_source, bool p_allow_replace = true);
	void AppendKeysAndValuesFrom(EidosDictionaryUnretained *p_source, bool p_require_column_match = false);
	void AddJSONFrom(nlohmann::json &json);
	
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_addKeysAndValuesFrom(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_appendKeysAndValuesFrom(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_clearKeysAndValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_compactIndices(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_getRowValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_identicalContents(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_serialize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_setValue(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class EidosDictionaryUnretained_Class : public EidosClass
{
private:
	typedef EidosClass super;

public:
	EidosDictionaryUnretained_Class(const EidosDictionaryUnretained_Class &p_original) = delete;	// no copy-construct
	EidosDictionaryUnretained_Class& operator=(const EidosDictionaryUnretained_Class&) = delete;	// no copying
	inline EidosDictionaryUnretained_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#pragma mark -
#pragma mark EidosDictionaryRetained
#pragma mark -

extern EidosClass *gEidosDictionaryRetained_Class;

// A base class for EidosObject subclasses that are under retain/release.
// This class is known in Eidos as "Dictionary"
class EidosDictionaryRetained : public EidosDictionaryUnretained
{
private:
	typedef EidosDictionaryUnretained super;

private:
	uint32_t refcount_ = 1;				// start life with a refcount of 1; the allocator does not need to call Retain()
	
public:
	EidosDictionaryRetained(const EidosDictionaryRetained &p_original) = delete;		// no copy-construct
	EidosDictionaryRetained& operator=(const EidosDictionaryRetained&) = delete;		// no copying
	inline EidosDictionaryRetained(void) { }
	inline virtual ~EidosDictionaryRetained(void) override { }
	
	inline __attribute__((always_inline)) void Retain(void)
	{
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("EidosDictionaryRetained::Retain(): EidosDictionaryRetained refcount_ change");
		refcount_++;
	}

	inline __attribute__((always_inline)) void Release(void)
	{
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("EidosDictionaryRetained::Retain(): EidosDictionaryRetained refcount_ change");
		if ((--refcount_) == 0)
			SelfDelete();
	}
	
	inline __attribute__((always_inline)) void Release_PARALLEL(void)
	{
		// This version of Release() can be used in parallel code, but must be protected by a lock,
		// a critical region, etc., or used in a context where it is known that a race does not occur.
		if ((--refcount_) == 0)
			SelfDelete();
	}
	
	virtual void SelfDelete(void);
	
	// construct from Eidos arguments; shared with DataFrame; the caller must call ContentsChanged()
	void ConstructFromEidos(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter, const std::string &p_caller_name, const std::string &p_constructor_name);
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
};

class EidosDictionaryRetained_Class : public EidosDictionaryUnretained_Class
{
private:
	typedef EidosDictionaryUnretained_Class super;

public:
	EidosDictionaryRetained_Class(const EidosDictionaryRetained_Class &p_original) = delete;	// no copy-construct
	EidosDictionaryRetained_Class& operator=(const EidosDictionaryRetained_Class&) = delete;	// no copying
	inline EidosDictionaryRetained_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
	
	virtual bool UsesRetainRelease(void) const override;
};


#endif /* __Eidos__eidos_class_dictionary__ */
