//
//  eidos_class_Dictionary.h
//  Eidos
//
//  Created by Ben Haller on 10/12/20.
//  Copyright (c) 2020-2022 Philipp Messer.  All rights reserved.
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


#include "eidos_globals.h"
#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
typedef robin_hood::unordered_flat_map<std::string, EidosValue_SP> EidosDictionaryHashTable;
#elif STD_UNORDERED_MAP_HASHING
#include <unordered_map>
typedef std::unordered_map<std::string, EidosValue_SP> EidosDictionaryHashTable;
#endif


#pragma mark -
#pragma mark EidosDictionaryUnretained
#pragma mark -

extern EidosClass *gEidosDictionaryUnretained_Class;

// This is a helper class for EidosDictionaryUnretained.  The purpose is to put all of its ivars into an allocated block,
// so that the overhead of inheriting from the class itself is only one pointer, unless the Dictionary functionality is
// actually used (which it usually isn't, since many SLiM objects inherit from Dictionary but rarely use it).
struct EidosDictionaryState
{
	// This hash table contains the values we are tracking.  Note that DictionarySymbols() should be
	// used by all code that does not need to modify the dictionary.
	EidosDictionaryHashTable dictionary_symbols_;
	
	// The dictionary_symbols_ table has no order for the keys.  We want to define an ordering; for
	// Dictionary the ordering is sorted, for DataFrame it is user-defined.  This vector determines
	// the user-visible ordering.  Whenever a new key is added, call KeyAddedToDictionary() to register it.
	// The SortedKeys() accessor should be used by all code that does not need to modify the keys.
	std::vector<std::string> sorted_keys_;
};

// This class is known in Eidos as "DictionaryBase"
class EidosDictionaryUnretained : public EidosObject
{
private:
	typedef EidosObject super;

protected:
	EidosDictionaryState *state_ptr_ = nullptr;
	
public:
	EidosDictionaryUnretained(const EidosDictionaryUnretained &p_original) = delete;				// no copy-construct
	EidosDictionaryUnretained& operator= (const EidosDictionaryUnretained &p_original) = delete;	// no assignment
	inline EidosDictionaryUnretained(void) { }
	
	inline virtual ~EidosDictionaryUnretained(void) override
	{
		if (state_ptr_)
		{
			delete state_ptr_;
			state_ptr_ = nullptr;
		}
	}
	
	// Whenever possible, access should go through these accessors to control modification of our symbols
	const EidosDictionaryHashTable *DictionarySymbols(void) const { return state_ptr_ ? &(state_ptr_->dictionary_symbols_) : nullptr; }
	const std::vector<std::string> *SortedKeys(void) const { return state_ptr_ ? &(state_ptr_->sorted_keys_) : nullptr; }
	
	// This method must be called whenever a key is added to the DictionarySymbols(), to add it to SortedKeys() correctly
	// The correct way to add new keys is different for Dictionary than for DataFrame, so always use this accessor
	virtual void KeyAddedToDictionary(const std::string &p_key);
	
	// This method must be called at the end of any code that changes the contents of the dictionary; it checks several invariants
	// Low-level accessors (RemoveAllKeys(), SetKeyValue(), etc.) should *not* call this; the top-level code controlling the change should
	virtual void ContentsChanged(const std::string &p_operation_name);
	
	int KeyCount(void) const { const std::vector<std::string> *keys = SortedKeys(); return keys ? (int)keys->size() : 0; }
	virtual EidosValue_SP AllKeys(void) const;
	
	std::string Serialization_SLiM(void) const;
	EidosValue_SP Serialization_CSV(std::string p_delimiter) const;
	virtual nlohmann::json JSONRepresentation(void) const override;
	
	// Non-const methods: callers of these methods must ensure that ContentsChanged() is called!
	inline __attribute__((always_inline)) void RemoveAllKeys(void)
	{
		if (state_ptr_)
		{
			// We keep state_ptr_ allocated to try to avoid allocation thrash
			state_ptr_->dictionary_symbols_.clear();
			state_ptr_->sorted_keys_.clear();
		}
	}
	
	void SetKeyValue(const std::string &key, EidosValue_SP value);
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
		refcount_++;
	}

	inline __attribute__((always_inline)) void Release(void)
	{
		if ((--refcount_) == 0)
			SelfDelete();
	}
	
	virtual void SelfDelete(void);
	
	// construct from Eidos arguments; shared with DataFrame; the caller must call ContentsChanged()
	void ConstructFromEidos(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter, std::string p_caller_name, std::string p_constructor_name);
	
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
