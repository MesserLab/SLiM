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


#include "eidos_value.h"

#pragma mark -
#pragma mark EidosDictionaryUnretained
#pragma mark -

extern EidosObjectClass *gEidos_EidosDictionaryUnretained_Class;


class EidosDictionaryUnretained : public EidosObjectElement
{
private:
	// We keep a pointer to our hash table for values we are tracking.  The reason to use a pointer is
	// that most clients of SLiM will not use getValue()/setValue() for most objects most of the time,
	// so we want to keep that case as minimal as possible in terms of speed and memory footprint.
	// Those who do use getValue()/setValue() will pay a little additional cost; that's OK.
	std::unordered_map<std::string, EidosValue_SP> *hash_symbols_ = nullptr;
	
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
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_setValue(EidosObjectElement **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};


class EidosDictionaryUnretained_Class : public EidosObjectClass
{
public:
	EidosDictionaryUnretained_Class(const EidosDictionaryUnretained_Class &p_original) = delete;	// no copy-construct
	EidosDictionaryUnretained_Class& operator=(const EidosDictionaryUnretained_Class&) = delete;	// no copying
	inline EidosDictionaryUnretained_Class(void) { }
	
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#pragma mark -
#pragma mark EidosDictionaryRetained
#pragma mark -

// A base class for EidosObjectElement subclasses that are under retain/release.  These must inherit from EidosDictionaryUnretained.
class EidosDictionaryRetained : public EidosDictionaryUnretained
{
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
};

class EidosDictionaryRetained_Class : public EidosDictionaryUnretained_Class
{
public:
	EidosDictionaryRetained_Class(const EidosDictionaryRetained_Class &p_original) = delete;	// no copy-construct
	EidosDictionaryRetained_Class& operator=(const EidosDictionaryRetained_Class&) = delete;	// no copying
	inline EidosDictionaryRetained_Class(void) { }
	
	virtual const std::string &ElementType(void) const override;
	
	virtual bool UsesRetainRelease(void) const override;
};


#pragma mark -
#pragma mark Inlines
#pragma mark -

// These inline implementations belong to EidosValue_Object_vector, but relate tightly to EidosDictionaryRetained,
// so we have to provide them here.  So that they are available to everyone that includes eidos_value.h, it includes us.

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element_CRR(EidosObjectElement *p_object)
{
	if (count_ == capacity_)
		expand();
	
	DeclareClassFromElement(p_object);
	
	if (class_uses_retain_release_)
		static_cast<EidosDictionaryRetained *>(p_object)->Retain();		// unsafe cast to avoid virtual function overhead
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element_RR(EidosObjectElement *p_object)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (!class_uses_retain_release_) RaiseForRetainReleaseViolation();
#endif
	
	if (count_ == capacity_)
		expand();
	
	DeclareClassFromElement(p_object);
	
	static_cast<EidosDictionaryRetained *>(p_object)->Retain();		// unsafe cast to avoid virtual function overhead
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element_NORR(EidosObjectElement *p_object)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (class_uses_retain_release_) RaiseForRetainReleaseViolation();
#endif
	
	if (count_ == capacity_)
		expand();
	
	DeclareClassFromElement(p_object);
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element_no_check_CRR(EidosObjectElement *p_object)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (count_ == capacity_) RaiseForCapacityViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
#endif
	
	if (class_uses_retain_release_)
		static_cast<EidosDictionaryRetained *>(p_object)->Retain();		// unsafe cast to avoid virtual function overhead
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element_no_check_RR(EidosObjectElement *p_object)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (count_ == capacity_) RaiseForCapacityViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
	if (!class_uses_retain_release_) RaiseForRetainReleaseViolation();
#endif
	
	static_cast<EidosDictionaryRetained *>(p_object)->Retain();		// unsafe cast to avoid virtual function overhead
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::push_object_element_no_check_NORR(EidosObjectElement *p_object)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (count_ == capacity_) RaiseForCapacityViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
	if (class_uses_retain_release_) RaiseForRetainReleaseViolation();
#endif
	
	values_[count_++] = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::set_object_element_no_check_CRR(EidosObjectElement *p_object, size_t p_index)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (p_index >= count_) RaiseForRangeViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
#endif
	EidosObjectElement *&value_slot_to_replace = values_[p_index];
	
	if (class_uses_retain_release_)
	{
		static_cast<EidosDictionaryRetained *>(p_object)->Retain();						// unsafe cast to avoid virtual function overhead
		if (value_slot_to_replace)
			static_cast<EidosDictionaryRetained *>(value_slot_to_replace)->Release();	// unsafe cast to avoid virtual function overhead
	}
	
	value_slot_to_replace = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::set_object_element_no_check_RR(EidosObjectElement *p_object, size_t p_index)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (p_index >= count_) RaiseForRangeViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
	if (!class_uses_retain_release_) RaiseForRetainReleaseViolation();
#endif
	EidosObjectElement *&value_slot_to_replace = values_[p_index];
	
	static_cast<EidosDictionaryRetained *>(p_object)->Retain();						// unsafe cast to avoid virtual function overhead
	if (value_slot_to_replace)
		static_cast<EidosDictionaryRetained *>(value_slot_to_replace)->Release();	// unsafe cast to avoid virtual function overhead
	
	value_slot_to_replace = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::set_object_element_no_check_no_previous_RR(EidosObjectElement *p_object, size_t p_index)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (p_index >= count_) RaiseForRangeViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
	if (!class_uses_retain_release_) RaiseForRetainReleaseViolation();
#endif
	EidosObjectElement *&value_slot_to_replace = values_[p_index];
	
	static_cast<EidosDictionaryRetained *>(p_object)->Retain();						// unsafe cast to avoid virtual function overhead
	
	value_slot_to_replace = p_object;
}

inline __attribute__((always_inline)) void EidosValue_Object_vector::set_object_element_no_check_NORR(EidosObjectElement *p_object, size_t p_index)
{
#if DEBUG
	// do checks only in DEBUG mode, for speed; the user should never be able to trigger these errors
	if (p_index >= count_) RaiseForRangeViolation();
	DeclareClassFromElement(p_object, true);				// require a prior matching declaration
	if (class_uses_retain_release_) RaiseForRetainReleaseViolation();
#endif
	EidosObjectElement *&value_slot_to_replace = values_[p_index];
	
	value_slot_to_replace = p_object;
}


#endif /* __Eidos__eidos_class_dictionary__ */
