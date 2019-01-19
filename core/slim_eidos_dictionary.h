//
//  slim_eidos_dictionary.h
//  SLiM
//
//  Created by Ben Haller on 12/6/16.
//  Copyright (c) 2016-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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
 
 The class SLiMEidosDictionary is a superclass for several classes in SLiM that defines a dictionary-style setValue()/getValue()
 Eidos interface that allows the users of those classes to store arbitrary (non-object) values under arbitrary string keys.
 
 */

#ifndef __SLiM__slim_eidos_dictionary__
#define __SLiM__slim_eidos_dictionary__


#include <string>
#include <unordered_map>

#include "eidos_global.h"
#include "eidos_value.h"
#include "eidos_interpreter.h"


extern EidosObjectClass *gSLiM_SLiMEidosDictionary_Class;


class SLiMEidosDictionary : public EidosObjectElement
{
private:
	// We keep a pointer to our hash table for values we are tracking.  The reason to use a pointer is
	// that most clients of SLiM will not use getValue()/setValue() for most objects most of the time,
	// so we want to keep that case as minimal as possible in terms of speed and memory footprint.
	// Those who do use getValue()/setValue() will pay a little additional cost; that's OK.
	std::unordered_map<std::string, EidosValue_SP> *hash_symbols_ = nullptr;
	
public:
	SLiMEidosDictionary(const SLiMEidosDictionary &p_original);
	SLiMEidosDictionary& operator= (const SLiMEidosDictionary &p_original) = delete;	// no assignment
	inline SLiMEidosDictionary(void) { }
	
	inline virtual ~SLiMEidosDictionary(void)
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
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	static EidosValue_SP ExecuteMethod_Accelerated_setValue(EidosObjectElement **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


class SLiMEidosDictionary_Class : public EidosObjectClass
{
public:
	SLiMEidosDictionary_Class(const SLiMEidosDictionary_Class &p_original) = delete;	// no copy-construct
	SLiMEidosDictionary_Class& operator=(const SLiMEidosDictionary_Class&) = delete;	// no copying
	inline SLiMEidosDictionary_Class(void) { }
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
};


#endif /* __SLiM__slim_eidos_dictionary__ */




































