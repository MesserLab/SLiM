//
//  script_pathproxy.cpp
//  SLiM
//
//  Created by Ben Haller on 5/1/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#include "script_pathproxy.h"
#include "script_functions.h"
#include "slim_global.h"

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
//	ScriptValue_PathProxy
//
#pragma mark ScriptValue_PathProxy

ScriptValue_PathProxy::ScriptValue_PathProxy(void) : base_path_("~")
{
}

ScriptValue_PathProxy::ScriptValue_PathProxy(std::string p_base_path) : base_path_(p_base_path)
{
}

std::string ScriptValue_PathProxy::ProxyType(void) const
{
	return "Path";
}

ScriptValue *ScriptValue_PathProxy::CopyValues(void) const
{
	return new ScriptValue_PathProxy(base_path_);
}

ScriptValue *ScriptValue_PathProxy::NewMatchingType(void) const
{
	return new ScriptValue_PathProxy(base_path_);
}

std::vector<std::string> ScriptValue_PathProxy::ReadOnlyMembers(void) const
{
	static std::vector<std::string> members;
	static bool been_here = false;
	
	// put hard-coded constants at the top of the list
	if (!been_here)
	{
		members.push_back("files");
		
		been_here = true;
	}
	
	return members;
}

std::vector<std::string> ScriptValue_PathProxy::ReadWriteMembers(void) const
{
	static std::vector<std::string> members;
	static bool been_here = false;
	
	// put hard-coded constants at the top of the list
	if (!been_here)
	{
		members.push_back("path");
		
		been_here = true;
	}
	
	return members;
}

std::string ScriptValue_PathProxy::ResolvedBasePath(void) const
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

ScriptValue *ScriptValue_PathProxy::GetValueForMember(const std::string &p_member_name) const
{
	if (p_member_name.compare("path") == 0)
		return new ScriptValue_String(base_path_);
	
	if (p_member_name.compare("files") == 0)
	{
		string path = ResolvedBasePath();
		
		// this code modified from GNU: http://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html#Simple-Directory-Lister
		// I'm not sure if it works on Windows... sigh...
		DIR *dp;
		struct dirent *ep;
		
		dp = opendir(path.c_str());
		
		if (dp != NULL)
		{
			ScriptValue_String *return_string = new ScriptValue_String();
			
			while ((ep = readdir(dp)))
				return_string->PushString(ep->d_name);
			
			(void)closedir(dp);
			
			return return_string;
		}
		else
		{
			// would be nice to emit an error message, but at present we don't have access to the stream...
			//p_output_stream << "Path " << path << " could not be opened." << endl;
			return ScriptValue_NULL::ScriptValue_NULL_Invisible();
		}
	}
	
	// FIXME could return superclass call, and the superclass could implement ls
	
	SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::GetValueForMember): no member '" << p_member_name << "'." << endl << slim_terminate();
	
	return nullptr;
}

void ScriptValue_PathProxy::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	ScriptValueType value_type = p_value->Type();
	int value_count = p_value->Count();
	
	if (p_member_name.compare("path") == 0)
	{
		if (value_type != ScriptValueType::kValueString)
			SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): type mismatch in assignment to member 'directory'." << endl << slim_terminate();
		if (value_count != 1)
			SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): value of size() == 1 expected in assignment to member 'directory'." << endl << slim_terminate();
		
		base_path_ = p_value->StringAtIndex(0);
	}
	else if (p_member_name.compare("files") == 0)
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): member '" << p_member_name << "' is read-only." << endl << slim_terminate();
	}
	else
	{
		SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::SetValueForMember): no member '" << p_member_name << "'." << endl << slim_terminate();
	}
}

std::vector<std::string> ScriptValue_PathProxy::Methods(void) const
{
	std::vector<std::string> methods = ScriptValue_Proxy::Methods();
	
	methods.push_back("readFile");
	methods.push_back("writeFile");
	
	return methods;
}

const FunctionSignature *ScriptValue_PathProxy::SignatureForMethod(std::string const &p_method_name) const
{
	// Signatures are all preallocated, for speed
	static FunctionSignature *readFileSig = nullptr;
	static FunctionSignature *writeFileSig = nullptr;
	
	if (!readFileSig)
	{
		readFileSig = (new FunctionSignature("readFile", FunctionIdentifier::kNoFunction, ScriptValueType::kValueString))->AddString();
		writeFileSig = (new FunctionSignature("writeFile", FunctionIdentifier::kNoFunction, ScriptValueType::kValueNULL))->AddString()->AddString();
	}
	
	if (p_method_name.compare("readFile") == 0)
		return readFileSig;
	else if (p_method_name.compare("writeFile") == 0)
		return writeFileSig;
	else
		return ScriptValue_Proxy::SignatureForMethod(p_method_name);
}

ScriptValue *ScriptValue_PathProxy::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	if (p_method_name.compare("readFile") == 0)
	{
		// the first argument is the filename
		ScriptValue *arg1_value = p_arguments[0];
		int arg1_count = arg1_value->Count();
		
		if (arg1_count != 1)
			SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::ExecuteMethod): method " << p_method_name << "() requires that its first argument's size() == 1." << endl << slim_terminate();
		
		string filename = arg1_value->StringAtIndex(0);
		string file_path = ResolvedBasePath() + "/" + filename;
		
		// read the contents in
		std::ifstream file_stream(file_path.c_str());
		
		if (!file_stream.is_open())
		{
			// not a fatal error, just a warning log
			p_output_stream << "WARNING: File at path " << file_path << " could not be read." << endl;
			return ScriptValue_NULL::ScriptValue_NULL_Invisible();
		}
		
		ScriptValue_String *string_result = new ScriptValue_String();
		string line;
		
		while (getline(file_stream, line))
			string_result->PushString(line);
		
		if (file_stream.bad())
		{
			// not a fatal error, just a warning log
			p_output_stream << "WARNING: Stream errors occurred while reading file at path " << file_path << "." << endl;
		}
		
		return string_result;
	}
	else if (p_method_name.compare("writeFile") == 0)
	{
		// the first argument is the filename
		ScriptValue *arg1_value = p_arguments[0];
		int arg1_count = arg1_value->Count();
		
		if (arg1_count != 1)
			SLIM_TERMINATION << "ERROR (ScriptValue_PathProxy::ExecuteMethod): method " << p_method_name << "() requires that its first argument's size() == 1." << endl << slim_terminate();
		
		string filename = arg1_value->StringAtIndex(0);
		string file_path = ResolvedBasePath() + "/" + filename;
		
		// the second argument is the file contents to write
		ScriptValue *arg2_value = p_arguments[1];
		int arg2_count = arg2_value->Count();
		
		// write the contents out
		std::ofstream file_stream(file_path.c_str());
		
		if (!file_stream.is_open())
		{
			// Not a fatal error, just a warning log
			p_output_stream << "WARNING (ScriptValue_PathProxy::ExecuteMethod): File at path " << file_path << " could not be opened." << endl;
			return ScriptValue_NULL::ScriptValue_NULL_Invisible();
		}
		
		for (int value_index = 0; value_index < arg2_count; ++value_index)
		{
			if (value_index > 0)
				file_stream << endl;
			
			file_stream << arg2_value->StringAtIndex(value_index);
		}
		
		if (file_stream.bad())
		{
			// Not a fatal error, just a warning log
			p_output_stream << "WARNING (ScriptValue_PathProxy::ExecuteMethod): Stream errors occurred while reading file at path " << file_path << "." << endl;
		}
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	else
	{
		return ScriptValue_Proxy::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
	}
}



































































