//
//  log_file.h
//  SLiM
//
//  Created by Ben Haller on 11/2/20.
//  Copyright (c) 2020-2021 Philipp Messer.  All rights reserved.
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

#ifndef log_file_h
#define log_file_h

#include "eidos_value.h"
#include "slim_globals.h"

#include <string>

class SLiMSim;


extern EidosClass *gSLiM_LogFile_Class;


// Built-in and custom generator types that are presently supported
enum class LogFileGeneratorType
{
	kGenerator_Generation,
	kGenerator_GenerationStage,
	kGenerator_PopulationSexRatio,
	kGenerator_PopulationSize,
	kGenerator_SubpopulationSexRatio,
	kGenerator_SubpopulationSize,
	kGenerator_CustomScript,
	kGenerator_CustomMeanAndSD			// results in two columns!
};

struct LogFileGeneratorInfo
{
	LogFileGeneratorType type_;			// the generator's type, as above
	EidosScript *script_;				// a script to execute to generate the data, or nullptr
	slim_objectid_t objectid_;			// the identifier for whatever object type might be relevant, or -1
	EidosValue_SP context_;				// the context value for the generator, if any
};


class LogFile : public EidosDictionaryRetained
{
private:
	typedef EidosDictionaryRetained super;

private:
	SLiMSim &sim_;												// UNOWNED POINTER: the simulation object we're working with
	
	std::string user_file_path_;								// the one given by the user to us
	std::string resolved_file_path_;							// the path we use internally
	
	bool header_logged_ = false;								// true if the header has been written out (in which case our generators are locked)
	
	bool compress_;
	std::string sep_;											// the separator string between values, such as "," or "\t"
	
	bool autologging_enabled_ = false;							// an overall flag to enable/disable automatic logging
	int64_t log_interval_ = 0;									// generation interval for automatic logging
	slim_generation_t autolog_start_ = 0;						// the first generation in which autologging occurred
	
	bool explicit_flushing_ = false;							// an overall flag to enable/disable flushing by number of rows
	int64_t flush_interval_ = 0;								// the maximum number of logged rows before we flush
	int64_t unflushed_row_count_ = 0;							// a running counter since the last flush
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;			// a user-defined tag value
	
	// Generators; these generate the data in the log file
	std::vector<LogFileGeneratorInfo> generator_info_;
	
	// Columns; note that one generator can generate more than one column!
	std::vector<std::string> column_names_;
	
	void RaiseForLockedHeader(const std::string &p_caller_name);
	
	EidosValue_SP _GeneratedValue_Generation(const LogFileGeneratorInfo &p_generator_info);
	EidosValue_SP _GeneratedValue_GenerationStage(const LogFileGeneratorInfo &p_generator_info);
	EidosValue_SP _GeneratedValue_PopulationSexRatio(const LogFileGeneratorInfo &p_generator_info);
	EidosValue_SP _GeneratedValue_PopulationSize(const LogFileGeneratorInfo &p_generator_info);
	EidosValue_SP _GeneratedValue_SubpopulationSexRatio(const LogFileGeneratorInfo &p_generator_info);
	EidosValue_SP _GeneratedValue_SubpopulationSize(const LogFileGeneratorInfo &p_generator_info);
	EidosValue_SP _GeneratedValue_CustomScript(const LogFileGeneratorInfo &p_generator_info);
	void _GeneratedValues_CustomMeanAndSD(const LogFileGeneratorInfo &p_generator_info, EidosValue_SP *p_generated_value_1, EidosValue_SP *p_generated_value_2);
	
public:
	LogFile(const LogFile &p_original) = delete;	// no copy-construct
	LogFile& operator=(const LogFile&) = delete;	// no copying
	
	explicit LogFile(SLiMSim &p_sim);
	virtual ~LogFile(void) override;
	
	void ConfigureFile(const std::string &p_filePath, std::vector<const std::string *> &p_initialContents, bool p_append, bool p_compress, const std::string &p_sep);
	void SetLogInterval(bool p_autologging_enabled, int64_t p_logInterval);
	void SetFlushInterval(bool p_explicit_flushing, int64_t p_flushInterval);
	
	void AppendNewRow(void);
	void GenerationEndCallout(void);
	
	virtual EidosValue_SP AllKeys(void) const override;	// provide keys in column order
	
	//
	// Eidos support
	//
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
	// Our own methods
	EidosValue_SP ExecuteMethod_addCustomColumn(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addGeneration(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addGenerationStage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addMeanSDColumns(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addPopulationSexRatio(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addPopulationSize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSubpopulationSexRatio(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSubpopulationSize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_flush(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_logRow(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setLogInterval(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setFilePath(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Overrides of Dictionary methods, since we have a special Dictionary behavior
	EidosValue_SP ExecuteMethod_addKeysAndValuesFrom(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_clearKeysAndValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

class LogFile_Class : public EidosDictionaryRetained_Class
{
private:
	typedef EidosDictionaryRetained_Class super;

public:
	LogFile_Class(const LogFile_Class &p_original) = delete;	// no copy-construct
	LogFile_Class& operator=(const LogFile_Class &) = delete;	// no copying
	inline LogFile_Class(std::string p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* log_file_h */






































