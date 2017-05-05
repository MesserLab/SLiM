//
//  slim_eidos_dictionary.h
//  SLiM
//
//  Created by Ben Haller on 12/6/16.
//  Copyright (c) 2016 Philipp Messer.  All rights reserved.
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
	std::unordered_map<std::string, EidosValue_SP> hash_symbols_;
	bool hash_used_ = false;	// to avoid overhead when unused
	
public:
	SLiMEidosDictionary(const SLiMEidosDictionary &p_original);
	SLiMEidosDictionary& operator= (const SLiMEidosDictionary &p_original) = delete;	// no assignment
	SLiMEidosDictionary(void);
	~SLiMEidosDictionary(void);
	
	inline void RemoveAllKeys(void)
	{
		// We keep a flag to avoid overhead when we don't actually use hash_symbols_;
		// this is surprisingly slow even when the hash is empty!
		if (hash_used_)
		{
			hash_symbols_.clear();
			hash_used_ = false;
		}
	}
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};


class SLiMEidosDictionary_Class : public EidosObjectClass
{
public:
	SLiMEidosDictionary_Class(const SLiMEidosDictionary_Class &p_original) = delete;	// no copy-construct
	SLiMEidosDictionary_Class& operator=(const SLiMEidosDictionary_Class&) = delete;	// no copying
	
	SLiMEidosDictionary_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
};


#endif /* __SLiM__slim_eidos_dictionary__ */




































