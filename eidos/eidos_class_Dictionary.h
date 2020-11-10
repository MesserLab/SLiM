//
//  eidos_class_Dictionary.h
//  Eidos
//
//  Created by Ben Haller on 10/12/20.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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

// This class is known in Eidos as "DictionaryBase"
class EidosDictionaryUnretained : public EidosObject
{
private:
	typedef EidosObject super;

private:
	// We keep a pointer to our hash table for values we are tracking.  The reason to use a pointer is
	// that most clients of SLiM will not use getValue()/setValue() for most objects most of the time,
	// so we want to keep that case as minimal as possible in terms of speed and memory footprint.
	// Those who do use getValue()/setValue() will pay a little additional cost; that's OK.
	EidosDictionaryHashTable *hash_symbols_ = nullptr;
	
public:
	EidosDictionaryUnretained(const EidosDictionaryUnretained &p_original);
	EidosDictionaryUnretained& operator= (const EidosDictionaryUnretained &p_original) = delete;	// no assignment
	inline EidosDictionaryUnretained(void) { }
	
	inline virtual ~EidosDictionaryUnretained(void) override
	{
		if (hash_symbols_)
			delete hash_symbols_;
	}
	
	inline __attribute__((always_inline)) void RemoveAllKeys(void)
	{
		if (hash_symbols_)
			hash_symbols_->clear();
	}
	
	void SetKeyValue(const std::string &key, EidosValue_SP value);
	
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_addKeysAndValuesFrom(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_clearKeysAndValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_setValue(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};


class EidosDictionaryUnretained_Class : public EidosClass
{
private:
	typedef EidosClass super;

public:
	EidosDictionaryUnretained_Class(const EidosDictionaryUnretained_Class &p_original) = delete;	// no copy-construct
	EidosDictionaryUnretained_Class& operator=(const EidosDictionaryUnretained_Class&) = delete;	// no copying
	inline EidosDictionaryUnretained_Class(void) { }
	
	virtual const EidosClass *Superclass(void) const override;
	virtual const std::string &ElementType(void) const override;
	
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
	inline EidosDictionaryRetained(const EidosDictionaryRetained &p_original) : EidosDictionaryUnretained(p_original) { }
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
	inline EidosDictionaryRetained_Class(void) { }
	
	virtual const EidosClass *Superclass(void) const override;
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosFunctionSignature_CSP> *Functions(void) const override;
	
	virtual bool UsesRetainRelease(void) const override;
};


#endif /* __Eidos__eidos_class_dictionary__ */
