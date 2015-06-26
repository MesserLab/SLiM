//
//  script_pathelement.h
//  SLiM
//
//  Created by Ben Haller on 5/1/15.
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
 
 The class Script_PathElement is an object value class (i.e. a value class for ScriptValue_Object) that encapsulates a
 filesystem directory.  It is quite primitive; you can list contents, read a file, or white a file.  That functionality
 may be useful in itself, but the main purpose is as a proof of concept for SLiMscript's support of proxies, including
 instance variables, method calls, and instantiation.  SLiM's scriptability is based upon element objects like this.
 
 */

#ifndef __SLiM__script_pathelement__
#define __SLiM__script_pathelement__

#include "script_value.h"


class Script_PathElement : public ScriptObjectElementInternal
{
private:
	std::string base_path_;
	
public:
	Script_PathElement(const Script_PathElement &p_original) = delete;		// can copy-construct
	Script_PathElement& operator=(const Script_PathElement&) = delete;	// no copying
	
	Script_PathElement(void);
	explicit Script_PathElement(const std::string &p_base_path);
	
	virtual const std::string ElementType(void) const;
	
	virtual bool ExternallyOwned(void) const;
	virtual ScriptObjectElement *ScriptCopy(void);
	virtual void ScriptDelete(void) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name);
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const FunctionSignature *SignatureForMethod(const std::string &p_method_name) const;
	virtual ScriptValue *ExecuteMethod(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter);
	
	std::string ResolvedBasePath(void) const;
};


#endif /* defined(__SLiM__script_pathelement__) */





































































