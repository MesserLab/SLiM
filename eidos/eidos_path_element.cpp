//
//  eidos_path_element.cpp
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


#include "eidos_path_element.h"
#include "eidos_functions.h"
#include "eidos_call_signature.h"
#include "eidos_global.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <fstream>


using std::string;
using std::vector;
using std::endl;


//
//	Eidos_PathElement
//
#pragma mark Eidos_PathElement

Eidos_PathElement::Eidos_PathElement(void) : base_path_("~")
{
}

Eidos_PathElement::Eidos_PathElement(const std::string &p_base_path) : base_path_(p_base_path)
{
}

const std::string *Eidos_PathElement::ElementType(void) const
{
	return &gStr_Path;
}

EidosObjectElement *Eidos_PathElement::ScriptCopy(void)
{
	return new Eidos_PathElement(base_path_);
}

void Eidos_PathElement::ScriptDelete(void) const
{
	delete this;
}

std::vector<std::string> Eidos_PathElement::ReadOnlyMembers(void) const
{
	std::vector<std::string> members;
	
	return members;
}

std::vector<std::string> Eidos_PathElement::ReadWriteMembers(void) const
{
	std::vector<std::string> members;
	
	members.push_back(gStr_path);
	
	return members;
}

bool Eidos_PathElement::MemberIsReadOnly(EidosGlobalStringID p_member_id) const
{
	if (p_member_id == gID_path)
		return false;
	else
		return EidosObjectElement::MemberIsReadOnly(p_member_id);
}

EidosValue *Eidos_PathElement::GetValueForMember(EidosGlobalStringID p_member_id)
{
	if (p_member_id == gID_path)
		return new EidosValue_String(base_path_);
	
	// all others, including gID_none
	else
		return EidosObjectElement::GetValueForMember(p_member_id);
}

void Eidos_PathElement::SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	if (p_member_id == gID_path)
	{
		EidosValueType value_type = p_value->Type();
		int value_count = p_value->Count();
		
		if (value_type != EidosValueType::kValueString)
			EIDOS_TERMINATION << "ERROR (Eidos_PathElement::SetValueForMember): type mismatch in assignment to member 'path'." << eidos_terminate();
		if (value_count != 1)
			EIDOS_TERMINATION << "ERROR (Eidos_PathElement::SetValueForMember): value of size() == 1 expected in assignment to member 'path'." << eidos_terminate();
		
		base_path_ = p_value->StringAtIndex(0);
		return;
	}
	
	// all others, including gID_none
	else
		return EidosObjectElement::SetValueForMember(p_member_id, p_value);
}

std::vector<std::string> Eidos_PathElement::Methods(void) const
{
	std::vector<std::string> methods = EidosObjectElement::Methods();
	
	methods.push_back(gStr_files);
	methods.push_back(gStr_readFile);
	methods.push_back(gStr_writeFile);
	
	return methods;
}

const EidosMethodSignature *Eidos_PathElement::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *filesSig = nullptr;
	static EidosInstanceMethodSignature *readFileSig = nullptr;
	static EidosInstanceMethodSignature *writeFileSig = nullptr;
	
	if (!filesSig)
	{
		filesSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_files, kValueMaskString));
		readFileSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_readFile, kValueMaskString))->AddString_S("fileName");
		writeFileSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_writeFile, kValueMaskNULL))->AddString_S("fileName")->AddString("contents");
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_files:
			return filesSig;
		case gID_readFile:
			return readFileSig;
		case gID_writeFile:
			return writeFileSig;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SignatureForMethod(p_method_id);
	}
}

std::string Eidos_PathElement::ResolvedBasePath(void) const
{
	string path = base_path_;
	
	// if there is a leading '~', replace it with the user's home directory; not sure if this works on Windows...
	if ((path.length() > 0) && (path.at(0) == '~'))
	{
		const char *homedir;
		
		if ((homedir = getenv("HOME")) == NULL)
			homedir = getpwuid(getuid())->pw_dir;
		
		if (strlen(homedir))
			path.replace(0, 1, homedir);
	}
	
	return path;
}

EidosValue *Eidos_PathElement::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_files:
		{
			string path = ResolvedBasePath();
			
			// this code modified from GNU: http://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html#Simple-Directory-Lister
			// I'm not sure if it works on Windows... sigh...
			DIR *dp;
			struct dirent *ep;
			
			dp = opendir(path.c_str());
			
			if (dp != NULL)
			{
				EidosValue_String *return_string = new EidosValue_String();
				
				while ((ep = readdir(dp)))
					return_string->PushString(ep->d_name);
				
				(void)closedir(dp);
				
				return return_string;
			}
			else
			{
				// would be nice to emit an error message, but at present we don't have access to the stream...
				//p_output_stream << "Path " << path << " could not be opened." << endl;
				return gStaticEidosValueNULLInvisible;
			}
		}
		case gID_readFile:
		{
			// the first argument is the filename
			EidosValue *arg0_value = p_arguments[0];
			int arg0_count = arg0_value->Count();
			
			if (arg0_count != 1)
				EIDOS_TERMINATION << "ERROR (Eidos_PathElement::ExecuteMethod): method " << StringForEidosGlobalStringID(p_method_id) << "() requires that its first argument's size() == 1." << eidos_terminate();
			
			string filename = arg0_value->StringAtIndex(0);
			string file_path = ResolvedBasePath() + "/" + filename;
			
			// read the contents in
			std::ifstream file_stream(file_path.c_str());
			
			if (!file_stream.is_open())
			{
				// not a fatal error, just a warning log
				p_interpreter.ExecutionOutputStream() << "WARNING: File at path " << file_path << " could not be read." << endl;
				return gStaticEidosValueNULLInvisible;
			}
			
			EidosValue_String *string_result = new EidosValue_String();
			string line;
			
			while (getline(file_stream, line))
				string_result->PushString(line);
			
			if (file_stream.bad())
			{
				// not a fatal error, just a warning log
				p_interpreter.ExecutionOutputStream() << "WARNING: Stream errors occurred while reading file at path " << file_path << "." << endl;
			}
			
			return string_result;
		}
		case gID_writeFile:
		{
			// the first argument is the filename
			EidosValue *arg0_value = p_arguments[0];
			int arg0_count = arg0_value->Count();
			
			if (arg0_count != 1)
				EIDOS_TERMINATION << "ERROR (Eidos_PathElement::ExecuteMethod): method " << StringForEidosGlobalStringID(p_method_id) << "() requires that its first argument's size() == 1." << eidos_terminate();
			
			string filename = arg0_value->StringAtIndex(0);
			string file_path = ResolvedBasePath() + "/" + filename;
			
			// the second argument is the file contents to write
			EidosValue *arg1_value = p_arguments[1];
			int arg1_count = arg1_value->Count();
			
			// write the contents out
			std::ofstream file_stream(file_path.c_str());
			
			if (!file_stream.is_open())
			{
				// Not a fatal error, just a warning log
				p_interpreter.ExecutionOutputStream() << "WARNING (Eidos_PathElement::ExecuteMethod): File at path " << file_path << " could not be opened." << endl;
				return gStaticEidosValueNULLInvisible;
			}
			
			for (int value_index = 0; value_index < arg1_count; ++value_index)
			{
				if (value_index > 0)
					file_stream << endl;
				
				file_stream << arg1_value->StringAtIndex(value_index);
			}
			
			if (file_stream.bad())
			{
				// Not a fatal error, just a warning log
				p_interpreter.ExecutionOutputStream() << "WARNING (Eidos_PathElement::ExecuteMethod): Stream errors occurred while reading file at path " << file_path << "." << endl;
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}



































































