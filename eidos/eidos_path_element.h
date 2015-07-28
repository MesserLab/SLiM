//
//  eidos_path_element.h
//  Eidos
//
//  Created by Ben Haller on 5/1/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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
 
 The class Eidos_PathElement is an object element class (i.e. an element class for EidosValue_Object) that encapsulates a
 filesystem directory.  It is quite primitive; you can list contents, read a file, or white a file.  That functionality
 may be useful in itself, but the main purpose is as a proof of concept for Eidos's support of object elements, including
 instance variables, method calls, and instantiation.
 
 */

#ifndef __Eidos__eidos_path_element__
#define __Eidos__eidos_path_element__

#include "eidos_value.h"


class Eidos_PathElement : public EidosObjectElementInternal
{
private:
	std::string base_path_;
	
public:
	Eidos_PathElement(const Eidos_PathElement &p_original) = delete;	// can copy-construct
	Eidos_PathElement& operator=(const Eidos_PathElement&) = delete;	// no copying
	
	Eidos_PathElement(void);
	explicit Eidos_PathElement(const std::string &p_base_path);
	
	virtual const std::string *ElementType(void) const;
	
	virtual EidosObjectElement *ScriptCopy(void);
	virtual void ScriptDelete(void) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual bool MemberIsReadOnly(EidosGlobalStringID p_member_id) const;
	virtual EidosValue *GetValueForMember(EidosGlobalStringID p_member_id);
	virtual void SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const EidosFunctionSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue *ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	std::string ResolvedBasePath(void) const;
};


#endif /* defined(__Eidos__eidos_path_element__) */





































































