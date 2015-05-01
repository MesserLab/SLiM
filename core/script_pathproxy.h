//
//  script_pathproxy.h
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
 
 The class ScriptValue_PathProxy is a proxy value class (i.e. a SLiMscript object class) that encapsulates the idea of a
 filesystem directory.  It is quite primitive; you can list contents, read a file, or white a file.  That functionality
 may be useful in itself, but the main purpose is as a proof of concept for SLiMscript's support of proxies, including
 instance variables, method calls, and instantiation.  SLiM's scriptability is based upon proxy objects like this.
 
 */

#ifndef __SLiM__script_pathproxy__
#define __SLiM__script_pathproxy__

#include "script_value.h"


class ScriptValue_PathProxy : public ScriptValue_Proxy
{
private:
	std::string base_path_;
	
public:
	ScriptValue_PathProxy(const ScriptValue_PathProxy &p_original) = delete;		// can copy-construct
	ScriptValue_PathProxy& operator=(const ScriptValue_PathProxy&) = delete;	// no copying
	
	ScriptValue_PathProxy(void);
	explicit ScriptValue_PathProxy(std::string p_base_path);
	
	std::string ProxyType(void) const;
	
	virtual ScriptValue *CopyValues(void) const;
	virtual ScriptValue *NewMatchingType(void) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual ScriptValue *GetValueForMember(const std::string &p_member_name) const;
	virtual void SetValueForMember(const std::string &p_member_name, ScriptValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual FunctionSignature SignatureForMethod(std::string const &p_method_name) const;
	virtual ScriptValue *ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter);
	
	std::string ResolvedBasePath(void) const;
};


#endif /* defined(__SLiM__script_pathproxy__) */





































































