//
//  eidos_functions_files.cpp
//  Eidos
//
//  Created by Ben Haller on 4/6/15; split from eidos_functions.cpp 09/26/2022
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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


#include "eidos_functions.h"
#include "eidos_interpreter.h"

#include <utility>
#include <string>
#include <vector>
#include <stdio.h>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>

#include "../eidos_zlib/zlib.h"


// ************************************************************************************
//
//	filesystem access functions
//
#pragma mark -
#pragma mark Filesystem access functions
#pragma mark -


//	(logical$)createDirectory(string$ path)
EidosValue_SP Eidos_ExecuteFunction_createDirectory(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *path_value = p_arguments[0].get();
	std::string base_path = path_value->StringAtIndex(0, nullptr);
	std::string error_string;
	bool success = Eidos_CreateDirectory(base_path, &error_string);
	
	// Emit a warning if there was one
	if (error_string.length())
		if (!gEidosSuppressWarnings)
			p_interpreter.ErrorOutputStream() << error_string << std::endl;
	
	return (success ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

//	(logical$)deleteFile(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_deleteFile(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	result_SP = ((remove(file_path.c_str()) == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	return result_SP;
}

//	(logical$)fileExists(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_fileExists(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	struct stat file_info;
	bool path_exists = (stat(file_path.c_str(), &file_info) == 0);
	
	result_SP = (path_exists ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	
	return result_SP;
}

//	(string)filesAtPath(string$ path, [logical$ fullPaths = F])
EidosValue_SP Eidos_ExecuteFunction_filesAtPath(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *path_value = p_arguments[0].get();
	std::string base_path = path_value->StringAtIndex(0, nullptr);
	int base_path_length = (int)base_path.length();
	bool base_path_ends_in_slash = (base_path_length > 0) && (base_path[base_path_length-1] == '/');
	std::string path = Eidos_ResolvedPath(base_path);
	bool fullPaths = p_arguments[1]->LogicalAtIndex(0, nullptr);
	
	// this code modified from GNU: http://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html#Simple-Directory-Lister
	// I'm not sure if it works on Windows... sigh...
	DIR *dp;
	
	dp = opendir(path.c_str());
	
	if (dp != NULL)
	{
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		while (true)
		{
			// BCH 5 April 2018: switching to readdir() instead of readdir_r().  It seemed like readdir_r() would be better in principle, since
			// it's thread-safe, but apparently it has been deprecated in POSIX because of several issues, and readdir() is now recommended
			// after all (and is thread-safe in most situations, supposedly, but if we ever go multithreaded that will need to be checked.)
			errno = 0;
			
			struct dirent *ep = readdir(dp);
			int error = errno;
			
			if (!ep && error)
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_filesAtPath): function filesAtPath() encountered error code " << error << " while iterating through path " << path << "." << std::endl;
				result_SP = gStaticEidosValueNULL;
				break;
			}
			
			if (!ep)
				break;
			
			std::string filename = ep->d_name;
			
			if (fullPaths)
				filename = base_path + (base_path_ends_in_slash ? "" : "/") + filename;		// NOLINT(*-inefficient-string-concatenation) : this is fine
			
			string_result->PushString(filename);
		}
		
		(void)closedir(dp);
	}
	else
	{
		if (!gEidosSuppressWarnings)
			p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_filesAtPath): function filesAtPath() could not open path " << path << "." << std::endl;
		result_SP = gStaticEidosValueNULL;
	}
	
	return result_SP;
}

//	(string$)getwd(void)
EidosValue_SP Eidos_ExecuteFunction_getwd(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	std::string cwd = Eidos_CurrentDirectory();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(cwd));
	
	return result_SP;
}

//	(string)readFile(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_readFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	// read the contents in
	std::ifstream file_stream(file_path.c_str());
	
	if (!file_stream.is_open())
	{
		if (!gEidosSuppressWarnings)
			p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_readFile): function readFile() could not read file at path " << file_path << "." << std::endl;
		result_SP = gStaticEidosValueNULL;
	}
	else
	{
		EidosValue_String_vector *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector();
		result_SP = EidosValue_SP(string_result);
		
		std::string line;
		
		while (getline(file_stream, line))
			string_result->PushString(line);
		
		if (file_stream.bad())
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_readFile): function readFile() encountered stream errors while reading file at path " << file_path << "." << std::endl;
			result_SP = gStaticEidosValueNULL;
		}
	}
	
	return result_SP;
}

//	(string$)setwd(string$ path)
EidosValue_SP Eidos_ExecuteFunction_setwd(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	// Get the path; this code is identical to getwd() above, except it makes the value invisible
	std::string cwd = Eidos_CurrentDirectory();
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(cwd));
	result_SP->SetInvisible(true);
	
	// Now set the path
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string final_path = Eidos_ResolvedPath(base_path);
	
	errno = 0;
	int retval = chdir(final_path.c_str());
	
	if (retval == -1)
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_setwd): the working directory could not be set (error " << errno << ")" << EidosTerminate(nullptr);
	
	return result_SP;
}

//	(string$)tempdir(void)
EidosValue_SP Eidos_ExecuteFunction_tempdir(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(Eidos_TemporaryDirectory()));
}

//	(logical$)flushFile(string$ filePath)
EidosValue_SP Eidos_ExecuteFunction_flushFile(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	// note that writeFile() adds ".gz" to the filename if compression is specified and it is not already present; we don't,
	// since we don't know if compression is on for this file; the user will therefore have to use the correct path
	
	// flush the contents out
	Eidos_FlushFile(file_path);
	
	return gStaticEidosValue_LogicalT;	// we used to return F if we had a warning condition; now those are errors, so we always return T
}

//	(logical$)writeFile(string$ filePath, string contents, [logical$ append = F], [logical$ compress = F])
EidosValue_SP Eidos_ExecuteFunction_writeFile(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string base_path = filePath_value->StringAtIndex(0, nullptr);
	std::string file_path = Eidos_ResolvedPath(base_path);
	
	// the second argument is the file contents to write, which we put into a vector
	EidosValue_String *contents_value = (EidosValue_String *)p_arguments[1].get();
	int contents_count = contents_value->Count();
	std::vector<const std::string *> contents_buffer;
	
	contents_buffer.reserve(contents_count);
	
	for (int value_index = 0; value_index < contents_count; ++value_index)
		contents_buffer.emplace_back(&contents_value->StringRefAtIndex(value_index, nullptr));
	
	// the third argument is an optional append flag, F by default
	bool append = p_arguments[2]->LogicalAtIndex(0, nullptr);
	
	// and then there is a flag for optional gzip compression
	bool do_compress = p_arguments[3]->LogicalAtIndex(0, nullptr);
	
	if (do_compress && !Eidos_string_hasSuffix(file_path, ".gz"))
		file_path.append(".gz");
	
	// write the contents out
	Eidos_WriteToFile(file_path, contents_buffer, append, do_compress, EidosFileFlush::kDefaultFlush);
	
#ifdef SLIMGUI
	// we need to provide SLiMgui with information about the file write we just did; this is gross, but it wants to know
	// we make a separate buffer for this purpose, with string copies, to donate to Community with &&
	{
		EidosContext *context = p_interpreter.Context();
		std::vector<std::string> slimgui_buffer;
		
		slimgui_buffer.reserve(contents_count);
		
		for (int value_index = 0; value_index < contents_count; ++value_index)
			slimgui_buffer.emplace_back(contents_value->StringRefAtIndex(value_index, nullptr));
		
		if (context)
			context->FileWriteNotification(file_path, std::move(slimgui_buffer), append);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeFile): (internal error) no Context in Eidos_ExecuteFunction_writeFile()." << EidosTerminate(nullptr);
	}
#endif
	
	return gStaticEidosValue_LogicalT;	// we used to return F if we had a warning condition; now those are errors, so we always return T
}

//	(string$)writeTempFile(string$ prefix, string$ suffix, string contents, [logical$ compress = F])
EidosValue_SP Eidos_ExecuteFunction_writeTempFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	if (!Eidos_TemporaryDirectoryExists())
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeTempFile): in function writeTempFile(), the temporary directory appears not to exist or is not writeable." << EidosTerminate(nullptr);
	
	EidosValue *prefix_value = p_arguments[0].get();
	std::string prefix = prefix_value->StringAtIndex(0, nullptr);
	EidosValue *suffix_value = p_arguments[1].get();
	std::string suffix = suffix_value->StringAtIndex(0, nullptr);
	
	// the third argument is the file contents to write
	EidosValue_String *contents_value = (EidosValue_String *)p_arguments[2].get();
	int contents_count = contents_value->Count();
	
	// and then there is a flag for optional gzip compression
	bool do_compress = p_arguments[3]->LogicalAtIndex(0, nullptr);
	
	if (do_compress && !Eidos_string_hasSuffix(suffix, ".gz"))
		suffix.append(".gz");
	
	// generate the filename template from the prefix/suffix
	std::string filename = prefix + "XXXXXX" + suffix;
	std::string file_path_template = Eidos_TemporaryDirectory() + filename;
	
	if ((filename.find('~') != std::string::npos) || (filename.find('/') != std::string::npos))
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeTempFile): in function writeTempFile(), prefix and suffix may not contain '~' or '/'; they may specify only a filename." << EidosTerminate(nullptr);
	
	// write the contents out; thanks to http://stackoverflow.com/questions/499636/how-to-create-a-stdofstream-to-a-temp-file for the temp file creation code
	char *file_path_cstr = strdup(file_path_template.c_str());
	int fd = Eidos_mkstemps(file_path_cstr, (int)suffix.length());
	
	if (fd == -1)
	{
		free(file_path_cstr);
		EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeTempFile): (internal error) Eidos_mkstemps() failed!" << EidosTerminate(nullptr);
	}
	
#ifdef SLIMGUI
	// we need to provide SLiMgui with information about the file write we just did; this is gross, but it wants to know
	// we make a separate buffer for this purpose, with string copies, to donate to Community with &&
	{
		EidosContext *context = p_interpreter.Context();
		std::string file_path(file_path_cstr);
		std::vector<std::string> slimgui_buffer;
		
		slimgui_buffer.reserve(contents_count);
		
		for (int value_index = 0; value_index < contents_count; ++value_index)
			slimgui_buffer.emplace_back(contents_value->StringRefAtIndex(value_index, nullptr));
		
		if (context)
			context->FileWriteNotification(file_path, std::move(slimgui_buffer), false);
		else
			EIDOS_TERMINATION << "ERROR (Eidos_ExecuteFunction_writeTempFile): (internal error) no Context in Eidos_ExecuteFunction_writeTempFile()." << EidosTerminate(nullptr);
	}
#endif
	
	if (do_compress)
	{
		// compression using zlib; very different from the no-compression case, unfortunately, because here we use C-based APIs
		gzFile gzf = gzdopen(fd, "wb");
		
		if (!gzf)
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() could not write to file at path " << file_path_cstr << "." << std::endl;
			result_SP = gStaticEidosValue_StringEmpty;
		}
		else
		{
			std::ostringstream outstream;
			
			if (contents_count == 1)
			{
				outstream << contents_value->StringAtIndex(0, nullptr);
			}
			else
			{
				const std::vector<std::string> &string_vec = *contents_value->StringVector();
				
				for (int value_index = 0; value_index < contents_count; ++value_index)
					outstream << string_vec[value_index] << std::endl;
			}
			
			std::string outstring = outstream.str();
			const char *outcstr = outstring.c_str();
			size_t outcstr_length = strlen(outcstr);
			
			// do the writing with zlib
			bool failed = true;
			int retval = gzbuffer(gzf, 128*1024L);	// bigger buffer for greater speed
			
			if (retval != -1)
			{
				retval = gzwrite(gzf, outcstr, (unsigned)outcstr_length);
				
				if ((retval != 0) || (outcstr_length == 0))	// writing 0 bytes returns 0, which is supposed to be an error code
				{
					retval = gzclose_w(gzf);
					
					if (retval == Z_OK)
						failed = false;
				}
			}
			
			if (failed)
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() encountered zlib errors while writing to file at path " << file_path_cstr << "." << std::endl;
				result_SP = gStaticEidosValue_StringEmpty;
			}
			else
			{
				std::string file_path(file_path_cstr);
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(file_path));
			}
		}
	}
	else
	{
		// no compression
		std::string file_path(file_path_cstr);
		std::ofstream file_stream(file_path.c_str(), std::ios_base::out);
		close(fd);	// opened by Eidos_mkstemps()
		
		if (!file_stream.is_open())
		{
			if (!gEidosSuppressWarnings)
				p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() could not write to file at path " << file_path << "." << std::endl;
			result_SP = gStaticEidosValue_StringEmpty;
		}
		else
		{
			if (contents_count == 1)
			{
				file_stream << contents_value->StringAtIndex(0, nullptr);	// no final newline in this case, so the user can precisely specify the file contents if desired
			}
			else
			{
				const std::vector<std::string> &string_vec = *contents_value->StringVector();
				
				for (int value_index = 0; value_index < contents_count; ++value_index)
				{
					file_stream << string_vec[value_index];
					
					// Add newlines after all lines but the last
					// BCH 27 January 2017: changed to add a newline after the last line, too, so appending new content to a file produces correct line breaks
					//if (value_index + 1 < contents_count)
					file_stream << std::endl;
				}
			}
			
			if (file_stream.bad())
			{
				if (!gEidosSuppressWarnings)
					p_interpreter.ErrorOutputStream() << "#WARNING (Eidos_ExecuteFunction_writeTempFile): function writeTempFile() encountered stream errors while writing to file at path " << file_path << "." << std::endl;
				result_SP = gStaticEidosValue_StringEmpty;
			}
			else
			{
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(file_path));
			}
		}
	}
	
	free(file_path_cstr);
	return result_SP;
}


















































