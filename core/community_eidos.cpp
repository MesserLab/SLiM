//
//  community_eidos.cpp
//  SLiM
//
//  Created by Ben Haller on 2/28/2022.
//  Copyright (c) 2022-2023 Philipp Messer.  All rights reserved.
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


#include "community.h"

#include "genome.h"
#include "individual.h"
#include "subpopulation.h"
#include "polymorphism.h"
#include "interaction_type.h"
#include "log_file.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <utility>
#include <algorithm>
#include <vector>
#include <cmath>
#include <ctime>
#include <unordered_map>


static std::string PrintBytes(size_t p_bytes)
{
	std::ostringstream sstream;
	
	sstream << p_bytes << " bytes";
	
	if (p_bytes > 1024.0 * 1024.0 * 1024.0 * 1024.0)
		sstream << " (" << (p_bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0)) << " TB" << ")";
	else if (p_bytes > 1024.0 * 1024.0 * 1024.0)
		sstream << " (" << (p_bytes / (1024.0 * 1024.0 * 1024.0)) << " GB" << ")";
	else if (p_bytes > 1024.0 * 1024.0)
		sstream << " (" << (p_bytes / (1024.0 * 1024.0)) << " MB" << ")";
	else if (p_bytes > 1024)
		sstream << " (" << (p_bytes / 1024.0) << " K" << ")";
	
	return sstream.str();
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

EidosValue_SP Community::ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_interpreter)
	
	// we only define initialize...() functions; so we must be in an initialize() callback
	if (tick_ != 0)
		EIDOS_TERMINATION << "ERROR (Community::ContextDefinedFunctionDispatch): the function " << p_function_name << "() may only be called in an initialize() callback." << EidosTerminate();
	
	// Non-species-specific initialization
	if (p_function_name.compare(gStr_initializeSLiMModelType) == 0)				return this->ExecuteContextFunction_initializeSLiMModelType(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeInteractionType) == 0)		return this->ExecuteContextFunction_initializeInteractionType(p_function_name, p_arguments, p_interpreter);
	
	// Species-specific initialization
	if (!active_species_)
		EIDOS_TERMINATION << "ERROR (Community::ContextDefinedFunctionDispatch): no active species in context-defined function dispatch; " << p_function_name << "() must be called from a species-specific initialize() callback." << EidosTerminate();
	
	if (p_function_name.compare(gStr_initializeAncestralNucleotides) == 0)		return active_species_->ExecuteContextFunction_initializeAncestralNucleotides(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeGenomicElement) == 0)		return active_species_->ExecuteContextFunction_initializeGenomicElement(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeGenomicElementType) == 0)	return active_species_->ExecuteContextFunction_initializeGenomicElementType(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeMutationType) == 0)			return active_species_->ExecuteContextFunction_initializeMutationType(p_function_name, p_arguments, p_interpreter);			// NOLINT(*-branch-clone) : intentional consecutive branches
	else if (p_function_name.compare(gStr_initializeMutationTypeNuc) == 0)		return active_species_->ExecuteContextFunction_initializeMutationType(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeRecombinationRate) == 0)	return active_species_->ExecuteContextFunction_initializeRecombinationRate(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeGeneConversion) == 0)		return active_species_->ExecuteContextFunction_initializeGeneConversion(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeMutationRate) == 0)			return active_species_->ExecuteContextFunction_initializeMutationRate(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeHotspotMap) == 0)			return active_species_->ExecuteContextFunction_initializeHotspotMap(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeSex) == 0)					return active_species_->ExecuteContextFunction_initializeSex(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeSLiMOptions) == 0)			return active_species_->ExecuteContextFunction_initializeSLiMOptions(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeSpecies) == 0)				return active_species_->ExecuteContextFunction_initializeSpecies(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeTreeSeq) == 0)				return active_species_->ExecuteContextFunction_initializeTreeSeq(p_function_name, p_arguments, p_interpreter);
	
	EIDOS_TERMINATION << "ERROR (Community::ContextDefinedFunctionDispatch): the function " << p_function_name << "() is not implemented by Community." << EidosTerminate();
}

const std::vector<EidosFunctionSignature_CSP> *Community::ZeroTickFunctionSignatures(void)
{
	// Allocate our own EidosFunctionSignature objects
	static std::vector<EidosFunctionSignature_CSP> sim_0_signatures_;
	
	if (!sim_0_signatures_.size())
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Community::ZeroTickFunctionSignatures(): not warmed up");
		
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeAncestralNucleotides, nullptr, kEidosValueMaskInt | kEidosValueMaskSingleton, "SLiM"))
									   ->AddIntString("sequence"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeGenomicElement, nullptr, kEidosValueMaskObject, gSLiM_GenomicElement_Class, "SLiM"))
										->AddIntObject("genomicElementType", gSLiM_GenomicElementType_Class)->AddInt("start")->AddInt("end"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeGenomicElementType, nullptr, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_GenomicElementType_Class, "SLiM"))
										->AddIntString_S("id")->AddIntObject("mutationTypes", gSLiM_MutationType_Class)->AddNumeric("proportions")->AddFloat_ON("mutationMatrix", gStaticEidosValueNULL));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeInteractionType, nullptr, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_InteractionType_Class, "SLiM"))
										->AddIntString_S("id")->AddString_S(gStr_spatiality)->AddLogical_OS(gStr_reciprocal, gStaticEidosValue_LogicalF)->AddNumeric_OS(gStr_maxDistance, gStaticEidosValue_FloatINF)->AddString_OS(gStr_sexSegregation, gStaticEidosValue_StringDoubleAsterisk));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeMutationType, nullptr, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class, "SLiM"))
									   ->AddIntString_S("id")->AddNumeric_S("dominanceCoeff")->AddString_S("distributionType")->AddEllipsis());
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeMutationTypeNuc, nullptr, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class, "SLiM"))
									   ->AddIntString_S("id")->AddNumeric_S("dominanceCoeff")->AddString_S("distributionType")->AddEllipsis());
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeRecombinationRate, nullptr, kEidosValueMaskVOID, "SLiM"))
										->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeGeneConversion, nullptr, kEidosValueMaskVOID, "SLiM"))
										->AddNumeric_S("nonCrossoverFraction")->AddNumeric_S("meanLength")->AddNumeric_S("simpleConversionFraction")->AddNumeric_OS("bias", gStaticEidosValue_Integer0)->AddLogical_OS("redrawLengthsOnFailure", gStaticEidosValue_LogicalF));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeMutationRate, nullptr, kEidosValueMaskVOID, "SLiM"))
										->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeHotspotMap, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddNumeric("multipliers")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSex, nullptr, kEidosValueMaskVOID, "SLiM"))
										->AddString_S("chromosomeType"));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSLiMOptions, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddLogical_OS("keepPedigrees", gStaticEidosValue_LogicalF)->AddString_OS("dimensionality", gStaticEidosValue_StringEmpty)->AddString_OS("periodicity", gStaticEidosValue_StringEmpty)->AddInt_OS("mutationRuns", gStaticEidosValue_Integer0)->AddLogical_OS("preventIncidentalSelfing", gStaticEidosValue_LogicalF)->AddLogical_OS("nucleotideBased", gStaticEidosValue_LogicalF)->AddLogical_OS("randomizeCallbacks", gStaticEidosValue_LogicalT));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSpecies, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddInt_OS("tickModulo", gStaticEidosValue_Integer1)->AddInt_OS("tickPhase", gStaticEidosValue_Integer1)->AddString_OS(gStr_avatar, gStaticEidosValue_StringEmpty)->AddString_OS("color", gStaticEidosValue_StringEmpty));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeTreeSeq, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddLogical_OS("recordMutations", gStaticEidosValue_LogicalT)->AddNumeric_OSN("simplificationRatio", gStaticEidosValueNULL)->AddInt_OSN("simplificationInterval", gStaticEidosValueNULL)->AddLogical_OS("checkCoalescence", gStaticEidosValue_LogicalF)->AddLogical_OS("runCrosschecks", gStaticEidosValue_LogicalF)->AddLogical_OS("retainCoalescentOnly", gStaticEidosValue_LogicalT)->AddString_OSN("timeUnit", gStaticEidosValueNULL));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSLiMModelType, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddString_S("modelType"));
	}
	
	return &sim_0_signatures_;
}

void Community::AddZeroTickFunctionsToMap(EidosFunctionMap &p_map)
{
	const std::vector<EidosFunctionSignature_CSP> *signatures = ZeroTickFunctionSignatures();
	
	if (signatures)
	{
		for (const EidosFunctionSignature_CSP &signature : *signatures)
			p_map.emplace(signature->call_name_, signature);
	}
}

void Community::RemoveZeroTickFunctionsFromMap(EidosFunctionMap &p_map)
{
	const std::vector<EidosFunctionSignature_CSP> *signatures = ZeroTickFunctionSignatures();
	
	if (signatures)
	{
		for (const EidosFunctionSignature_CSP &signature : *signatures)
			p_map.erase(signature->call_name_);
	}
}

void Community::AddSLiMFunctionsToMap(EidosFunctionMap &p_map)
{
	const std::vector<EidosFunctionSignature_CSP> *signatures = SLiMFunctionSignatures();
	
	if (signatures)
	{
		for (const EidosFunctionSignature_CSP &signature : *signatures)
			p_map.emplace(signature->call_name_, signature);
	}
}

EidosSymbolTable *Community::SymbolsFromBaseSymbols(EidosSymbolTable *p_base_symbols)
{
	// Since we keep our own symbol table long-term, this function does not actually re-derive a new table, but just returns the cached table
	if (p_base_symbols != gEidosConstantsSymbolTable)
		EIDOS_TERMINATION << "ERROR (Community::SymbolsFromBaseSymbols): (internal error) SLiM requires that its parent symbol table be the standard Eidos symbol table." << EidosTerminate();
	
	return simulation_constants_;
}

//	*********************	(void)initializeSLiMModelType(string$ modelType)
//
EidosValue_SP Community::ExecuteContextFunction_initializeSLiMModelType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_interpreter)
	EidosValue *arg_modelType_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_modeltype_declarations_ > 0)
		EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeSLiMModelType): initializeSLiMModelType() may be called only once." << EidosTerminate();
	
	if (is_explicit_species_ && active_species_)
		EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeSLiMModelType): in multispecies models, initializeSLiMModelType() may only be called from a non-species-specific (`species all`) initialize() callback." << EidosTerminate();
	
	if ((num_interaction_types_ > 0) ||
		(active_species_ &&
		 active_species_->HasDoneAnyInitialization()))
		EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeSLiMModelType): initializeSLiMModelType() must be called before all other initialization functions." << EidosTerminate();
	
	{
		// string$ modelType
		std::string model_type = arg_modelType_value->StringAtIndex(0, nullptr);
		
		if (model_type == "WF")
			SetModelType(SLiMModelType::kModelTypeWF);
		else if (model_type == "nonWF")
			SetModelType(SLiMModelType::kModelTypeNonWF);
		else
			EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeSLiMModelType): in initializeSLiMModelType(), legal values for parameter modelType are only 'WF' or 'nonWF'." << EidosTerminate();
	}
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeSLiMModelType(";
		
		// modelType
		output_stream << "modelType = ";
		
		if (model_type_ == SLiMModelType::kModelTypeWF) output_stream << "'WF'";
		else if (model_type_ == SLiMModelType::kModelTypeNonWF) output_stream << "'nonWF'";
		
		output_stream << ");" << std::endl;
	}
	
	num_modeltype_declarations_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(object<InteractionType>$)initializeInteractionType(is$ id, string$ spatiality, [logical$ reciprocal = F], [numeric$ maxDistance = INF], [string$ sexSegregation = "**"])
//
EidosValue_SP Community::ExecuteContextFunction_initializeInteractionType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *spatiality_value = p_arguments[1].get();
	EidosValue *reciprocal_value = p_arguments[2].get();
	EidosValue *maxDistance_value = p_arguments[3].get();
	EidosValue *sexSegregation_value = p_arguments[4].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (is_explicit_species_ && active_species_)
		EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeInteractionType): in multispecies models, initializeInteractionType() may only be called from a non-species-specific (`species all`) initialize() callback." << EidosTerminate();
	
	slim_objectid_t map_identifier = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 'i');
	std::string spatiality_string = spatiality_value->StringAtIndex(0, nullptr);
	bool reciprocal = reciprocal_value->LogicalAtIndex(0, nullptr);		// UNUSED
	double max_distance = maxDistance_value->FloatAtIndex(0, nullptr);
	std::string sex_string = sexSegregation_value->StringAtIndex(0, nullptr);
	IndividualSex receiver_sex = IndividualSex::kUnspecified, exerter_sex = IndividualSex::kUnspecified;
	
	if (InteractionTypeWithID(map_identifier))
		EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() interaction type m" << map_identifier << " already defined." << EidosTerminate();
	
	if (sex_string == "**")			{ receiver_sex = IndividualSex::kUnspecified;	exerter_sex = IndividualSex::kUnspecified;	}
	else if (sex_string == "*M")	{ receiver_sex = IndividualSex::kUnspecified;	exerter_sex = IndividualSex::kMale;			}
	else if (sex_string == "*F")	{ receiver_sex = IndividualSex::kUnspecified;	exerter_sex = IndividualSex::kFemale;		}
	else if (sex_string == "M*")	{ receiver_sex = IndividualSex::kMale;			exerter_sex = IndividualSex::kUnspecified;	}
	else if (sex_string == "MM")	{ receiver_sex = IndividualSex::kMale;			exerter_sex = IndividualSex::kMale;			}
	else if (sex_string == "MF")	{ receiver_sex = IndividualSex::kMale;			exerter_sex = IndividualSex::kFemale;		}
	else if (sex_string == "F*")	{ receiver_sex = IndividualSex::kFemale;		exerter_sex = IndividualSex::kUnspecified;	}
	else if (sex_string == "FM")	{ receiver_sex = IndividualSex::kFemale;		exerter_sex = IndividualSex::kMale;			}
	else if (sex_string == "FF")	{ receiver_sex = IndividualSex::kFemale;		exerter_sex = IndividualSex::kFemale;		}
	else
		EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() unsupported sexSegregation value (must be '**', '*M', '*F', 'M*', 'MM', 'MF', 'F*', 'FM', or 'FF')." << EidosTerminate();
	
	InteractionType *new_interaction_type = new InteractionType(*this, map_identifier, spatiality_string, reciprocal, max_distance, receiver_sex, exerter_sex);
	
	interaction_types_.emplace(map_identifier, new_interaction_type);
	interaction_types_changed_ = true;
	
	// define a new Eidos variable to refer to the new mutation type
	EidosSymbolTableEntry &symbol_entry = new_interaction_type->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (Community::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeInteractionType(" << map_identifier << ", \"" << spatiality_string << "\"";
		
		if (reciprocal == true)
			output_stream << ", reciprocal=T";
		
		if (!std::isinf(max_distance))
			output_stream << ", maxDistance=" << max_distance;
		
		if (sex_string != "**")
			output_stream << ", sexSegregation=\"" << sex_string << "\"";
		
		output_stream << ");" << std::endl;
	}
	
	num_interaction_types_++;
	return symbol_entry.second;
}

const EidosClass *Community::Class(void) const
{
	return gSLiM_Community_Class;
}

void Community::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP Community::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_allGenomicElementTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_GenomicElementType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto getype : all_genomic_element_types_)
				vec->push_object_element_NORR(getype.second);
			
			return result_SP;
		}
		case gID_allInteractionTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_InteractionType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto inttype : interaction_types_)
				vec->push_object_element_NORR(inttype.second);
			
			return result_SP;
		}
		case gID_allMutationTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_MutationType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto muttype : all_mutation_types_)
				vec->push_object_element_NORR(muttype.second);
			
			return result_SP;
		}
		case gID_allScriptBlocks:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_SLiMEidosBlock_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
			
			for (auto script_block : script_blocks)
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
					vec->push_object_element_NORR(script_block);
			
			return result_SP;
		}
		case gID_allSpecies:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Species_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto species : all_species_)
				vec->push_object_element_NORR(species);
			
			return result_SP;
		}
		case gID_allSubpopulations:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Subpopulation_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto species : all_species_)
				for (auto pop : species->population_.subpops_)
					vec->push_object_element_NORR(pop.second);
			
			return result_SP;
		}
		case gID_logFiles:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_LogFile_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
            for (LogFile *logfile : log_file_registry_)
				vec->push_object_element_RR(logfile);
			
			return result_SP;
		}
		case gID_modelType:
		{
			static EidosValue_SP static_model_type_string_WF;
			static EidosValue_SP static_model_type_string_nonWF;
			
#pragma omp critical (GetProperty_modelType_cache)
			{
				if (!static_model_type_string_WF)
				{
					static_model_type_string_WF = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("WF"));
					static_model_type_string_nonWF = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("nonWF"));
				}
			}
			
			switch (model_type_)
			{
				case SLiMModelType::kModelTypeWF:		return static_model_type_string_WF;
				case SLiMModelType::kModelTypeNonWF:	return static_model_type_string_nonWF;
				default:								return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
			
			// variables
		case gID_tick:
		{
			if (cached_value_tick_ && (((EidosValue_Int_singleton *)cached_value_tick_.get())->IntValue() != tick_))
				cached_value_tick_.reset();
			if (!cached_value_tick_)
				cached_value_tick_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tick_));
			return cached_value_tick_;
		}
		case gID_cycleStage:
		{
			SLiMCycleStage cycle_stage = CycleStage();
			std::string cycle_stage_str = StringForSLiMCycleStage(cycle_stage);
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(cycle_stage_str));
		}
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Community::GetProperty): property tag accessed on simulation object before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
		case gID_verbosity:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(SLiM_verbosity_level));
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void Community::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_tick:
		{
			int64_t value = p_value.IntAtIndex(0, nullptr);
			slim_tick_t old_tick = tick_;
			slim_tick_t new_tick = SLiMCastToTickTypeOrRaise(value);
			
			SetTick(new_tick);
			
			// Setting the tick into the future is generally harmless; the simulation logic is designed to handle that anyway, since
			// that happens every tick.  Setting the tick into the past is a bit tricker, since some things that have already
			// occurred need to be invalidated.  In particular, historical data cached by SLiMgui needs to be fixed.  Note that here we
			// do NOT remove Substitutions that are in the future, or otherwise try to backtrack the actual simulation state.  If the user
			// actually restores a past state with readFromPopulationFile(), all that kind of stuff will be reset; but if all they do is
			// set the tick counter back, the model state is unchanged, substitutions are still fixed, etc.  This means that the
			// simulation code needs to be robust to the possibility that some records, e.g. for Substitutions, may appear to be about
			// events in the future.  But usually users will only set the tick back if they also call readFromPopulationFile().
			if (tick_ < old_tick)
			{
#ifdef SLIMGUI
				// Fix fitness histories for SLiMgui.  Note that mutation_loss_times_ and mutation_fixation_times_ are not fixable, since
				// their entries are not separated out by tick, so we just leave them as is, containing information about
				// alternative futures of the model.
				for (Species *species : all_species_)
				{
					for (auto history_record_iter : species->population_.fitness_histories_)
					{
						FitnessHistory &history_record = history_record_iter.second;
						double *history = history_record.history_;
						
						if (history)
						{
							int old_last_valid_history_index = std::max(0, old_tick - 2);		// if tick==2, tick 1 was the last valid entry, and it is at index 0
							int new_last_valid_history_index = std::max(0, tick_ - 2);		// ditto
							
							// make sure that we don't overrun the end of the buffer
							if (old_last_valid_history_index > history_record.history_length_ - 1)
								old_last_valid_history_index = history_record.history_length_ - 1;
							
							for (int entry_index = new_last_valid_history_index + 1; entry_index <= old_last_valid_history_index; ++entry_index)
								history[entry_index] = NAN;
						}
					}
					
					for (auto history_record_iter : species->population_.subpop_size_histories_)
					{
						SubpopSizeHistory &history_record = history_record_iter.second;
						slim_popsize_t *history = history_record.history_;
						
						if (history)
						{
							int old_last_valid_history_index = std::max(0, old_tick - 2);		// if tick==2, tick 1 was the last valid entry, and it is at index 0
							int new_last_valid_history_index = std::max(0, tick_ - 2);		// ditto
							
							// make sure that we don't overrun the end of the buffer
							if (old_last_valid_history_index > history_record.history_length_ - 1)
								old_last_valid_history_index = history_record.history_length_ - 1;
							
							for (int entry_index = new_last_valid_history_index + 1; entry_index <= old_last_valid_history_index; ++entry_index)
								history[entry_index] = 0;
						}
					}
				}
#endif
			}
			
			return;
		}
			
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		case gID_verbosity:
		{
			int64_t value = p_value.IntAtIndex(0, nullptr);
			
			SLiM_verbosity_level = value;	// at the command line we bounds-check this, but here I see no need
			return;
		}
			
			// all others, including gID_none
		default:
			return super::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP Community::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_createLogFile:					return ExecuteMethod_createLogFile(p_method_id, p_arguments, p_interpreter);
		case gID_deregisterScriptBlock:			return ExecuteMethod_deregisterScriptBlock(p_method_id, p_arguments, p_interpreter);
		case gID_genomicElementTypesWithIDs:	return ExecuteMethod_genomicElementTypesWithIDs(p_method_id, p_arguments, p_interpreter);
		case gID_interactionTypesWithIDs:		return ExecuteMethod_interactionTypesWithIDs(p_method_id, p_arguments, p_interpreter);
		case gID_mutationTypesWithIDs:			return ExecuteMethod_mutationTypesWithIDs(p_method_id, p_arguments, p_interpreter);
		case gID_scriptBlocksWithIDs:			return ExecuteMethod_scriptBlocksWithIDs(p_method_id, p_arguments, p_interpreter);
		case gID_speciesWithIDs:				return ExecuteMethod_speciesWithIDs(p_method_id, p_arguments, p_interpreter);
		case gID_subpopulationsWithIDs:			return ExecuteMethod_subpopulationsWithIDs(p_method_id, p_arguments, p_interpreter);
		case gID_outputUsage:					return ExecuteMethod_outputUsage(p_method_id, p_arguments, p_interpreter);
		case gID_registerFirstEvent:
		case gID_registerEarlyEvent:
		case gID_registerLateEvent:				return ExecuteMethod_registerFirstEarlyLateEvent(p_method_id, p_arguments, p_interpreter);
		case gID_registerInteractionCallback:	return ExecuteMethod_registerInteractionCallback(p_method_id, p_arguments, p_interpreter);
		case gID_rescheduleScriptBlock:			return ExecuteMethod_rescheduleScriptBlock(p_method_id, p_arguments, p_interpreter);
		case gID_simulationFinished:			return ExecuteMethod_simulationFinished(p_method_id, p_arguments, p_interpreter);
		case gEidosID_usage:					return ExecuteMethod_usage(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (object<LogFile>$)createLogFile(string$ filePath, [Ns initialContents = NULL], [logical$ append = F], [logical$ compress = F], [string$ sep = ","], [Ni$ logInterval = NULL], [Ni$ flushInterval = NULL])
EidosValue_SP Community::ExecuteMethod_createLogFile(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_SP result_SP(nullptr);
	
	EidosValue_String *filePath_value = (EidosValue_String *)p_arguments[0].get();
	EidosValue *initialContents_value = p_arguments[1].get();
	EidosValue *append_value = p_arguments[2].get();
	EidosValue *compress_value = p_arguments[3].get();
	EidosValue_String *sep_value = (EidosValue_String *)p_arguments[4].get();
	EidosValue *logInterval_value = p_arguments[5].get();
	EidosValue *flushInterval_value = p_arguments[6].get();
	
	// process parameters
	const std::string &filePath = filePath_value->StringRefAtIndex(0, nullptr);
	std::vector<const std::string *> initialContents;
	bool append = append_value->LogicalAtIndex(0, nullptr);
	bool do_compress = compress_value->LogicalAtIndex(0, nullptr);
	const std::string &sep = sep_value->StringRefAtIndex(0, nullptr);
	bool autologging = false, explicitFlushing = false;
	int64_t logInterval = 0, flushInterval = 0;
	
	if (initialContents_value->Type() != EidosValueType::kValueNULL)
	{
		EidosValue_String *ic_string_value = (EidosValue_String *)initialContents_value;
		int ic_count = initialContents_value->Count();
		
		for (int ic_index = 0; ic_index < ic_count; ++ic_index)
			initialContents.emplace_back(&ic_string_value->StringRefAtIndex(ic_index, nullptr));
	}
	
	if (logInterval_value->Type() == EidosValueType::kValueNULL)
	{
		// NULL turns off autologging
		autologging = false;
		logInterval = 0;
	}
	else
	{
		autologging = true;
		logInterval = logInterval_value->IntAtIndex(0, nullptr);
	}
	
	if (flushInterval_value->Type() == EidosValueType::kValueNULL)
	{
		// NULL requests our automatic flushing behavior
		explicitFlushing = false;
		flushInterval = 0;
	}
	else
	{
		explicitFlushing = true;
		flushInterval = flushInterval_value->IntAtIndex(0, nullptr);
	}
	
	// Create the LogFile object
	LogFile *logfile = new LogFile(*this);
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(logfile, gSLiM_LogFile_Class));
	
	// Add it to our registry; it has a retain count from new that we will take over at this point
	log_file_registry_.emplace_back(logfile);
	
	// Configure it
	logfile->SetLogInterval(autologging, logInterval);
	logfile->SetFlushInterval(explicitFlushing, flushInterval);
	logfile->ConfigureFile(filePath, initialContents, append, do_compress, sep);
	
	// Check for duplicate LogFiles using the same path; this is a common error so I'm making it illegal
	const std::string &resolved_path = logfile->ResolvedFilePath();
	
	for (LogFile *existing_log_file : log_file_registry_)
	{
		if (existing_log_file != logfile)
		{
			const std::string &existing_path = existing_log_file->ResolvedFilePath();
			
			if (resolved_path == existing_path)
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_createLogFile): createLogFile() cannot create a new log file at " << resolved_path << " because an existing log file is already using that path." << EidosTerminate();
		}
	}
	
	return result_SP;
}

//	*********************	- (void)deregisterScriptBlock(io<SLiMEidosBlock> scriptBlocks)
//
EidosValue_SP Community::ExecuteMethod_deregisterScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *scriptBlocks_value = p_arguments[0].get();
	
	int block_count = scriptBlocks_value->Count();
	
	// We just schedule the blocks for deregistration; we do not deregister them immediately, because that would leave stale pointers lying around
	for (int block_index = 0; block_index < block_count; ++block_index)
	{
		SLiMEidosBlock *block = SLiM_ExtractSLiMEidosBlockFromEidosValue_io(scriptBlocks_value, block_index, this, nullptr, "deregisterScriptBlock()");	// agnostic to species
		
		if (block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
		{
			// this should never be hit, because the user should have no way to get a reference to a function block
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_deregisterScriptBlock): (internal error) deregisterScriptBlock() cannot be called on user-defined function script blocks." << EidosTerminate();
		}
		else if (block->type_ == SLiMEidosBlockType::SLiMEidosInteractionCallback)
		{
			// interaction() callbacks have to work differently, because they can be called at any time after an
			// interaction has been evaluated, up until the interaction is invalidated; we can't make pointers
			// to interaction() callbacks go stale except at that specific point in the cycle
			if (std::find(scheduled_interaction_deregs_.begin(), scheduled_interaction_deregs_.end(), block) != scheduled_interaction_deregs_.end())
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_deregisterScriptBlock): deregisterScriptBlock() called twice on the same script block." << EidosTerminate();
			
			scheduled_interaction_deregs_.emplace_back(block);
			
#if DEBUG_BLOCK_REG_DEREG
			std::cout << "deregisterScriptBlock() called for block:" << std::endl;
			std::cout << "   ";
			block->Print(std::cout);
			std::cout << std::endl;
#endif
		}
		else
		{
			// all other script blocks go on the main list and get cleared out at the end of each cycle stage
			if (std::find(scheduled_deregistrations_.begin(), scheduled_deregistrations_.end(), block) != scheduled_deregistrations_.end())
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_deregisterScriptBlock): deregisterScriptBlock() called twice on the same script block." << EidosTerminate();
			
			scheduled_deregistrations_.emplace_back(block);
			
#if DEBUG_BLOCK_REG_DEREG
			std::cout << "deregisterScriptBlock() called for block:" << std::endl;
			std::cout << "   ";
			block->Print(std::cout);
			std::cout << std::endl;
#endif
		}
		
#ifdef SLIMGUI
		gSLiMScheduling << "\t\tderegisterScriptBlock() called for block: ";
		block->PrintDeclaration(gSLiMScheduling, this);
		gSLiMScheduling << std::endl;
#endif
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	– (object<GenomicElementType>)genomicElementTypesWithIDs(integer ids)
//
EidosValue_SP Community::ExecuteMethod_genomicElementTypesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *ids_value = p_arguments[0].get();
	int ids_count = ids_value->Count();
	
	if (ids_count == 1)
	{
		// Singleton case
		slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(0, nullptr));
		GenomicElementType *object = GenomicElementTypeWithID(id);
		
		if (!object)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_genomicElementTypesWithIDs): genomicElementTypesWithIDs() did not find a genomic element type with id " << id << "." << EidosTerminate();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(object, gSLiM_GenomicElementType_Class));
	}
	else
	{
		// Vector case
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_GenomicElementType_Class))->resize_no_initialize_RR(ids_count);
		
		for (int id_index = 0; id_index < ids_count; id_index++)
		{
			slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(id_index, nullptr));
			GenomicElementType *object = GenomicElementTypeWithID(id);
			
			if (!object)
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_genomicElementTypesWithIDs): genomicElementTypesWithIDs() did not find a genomic element type with id " << id << "." << EidosTerminate();
			
			vec->set_object_element_no_check_NORR(object, id_index);
		}
		
		return EidosValue_SP(vec);
	}
}

//	*********************	– (object<InteractionType>)interactionTypesWithIDs(integer ids)
//
EidosValue_SP Community::ExecuteMethod_interactionTypesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *ids_value = p_arguments[0].get();
	int ids_count = ids_value->Count();
	
	if (ids_count == 1)
	{
		// Singleton case
		slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(0, nullptr));
		InteractionType *object = InteractionTypeWithID(id);
		
		if (!object)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_interactionTypesWithIDs): interactionTypesWithIDs() did not find an interaction type with id " << id << "." << EidosTerminate();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(object, gSLiM_InteractionType_Class));
	}
	else
	{
		// Vector case
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_InteractionType_Class))->resize_no_initialize_RR(ids_count);
		
		for (int id_index = 0; id_index < ids_count; id_index++)
		{
			slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(id_index, nullptr));
			InteractionType *object = InteractionTypeWithID(id);
			
			if (!object)
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_interactionTypesWithIDs): interactionTypesWithIDs() did not find an interaction type with id " << id << "." << EidosTerminate();
			
			vec->set_object_element_no_check_NORR(object, id_index);
		}
		
		return EidosValue_SP(vec);
	}
}

//	*********************	– (object<MutationType>)mutationTypesWithIDs(integer ids)
//
EidosValue_SP Community::ExecuteMethod_mutationTypesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *ids_value = p_arguments[0].get();
	int ids_count = ids_value->Count();
	
	if (ids_count == 1)
	{
		// Singleton case
		slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(0, nullptr));
		MutationType *object = MutationTypeWithID(id);
		
		if (!object)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_mutationTypesWithIDs): mutationTypesWithIDs() did not find a genomic element type with id " << id << "." << EidosTerminate();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(object, gSLiM_MutationType_Class));
	}
	else
	{
		// Vector case
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_MutationType_Class))->resize_no_initialize_RR(ids_count);
		
		for (int id_index = 0; id_index < ids_count; id_index++)
		{
			slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(id_index, nullptr));
			MutationType *object = MutationTypeWithID(id);
			
			if (!object)
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_mutationTypesWithIDs): mutationTypesWithIDs() did not find a genomic element type with id " << id << "." << EidosTerminate();
			
			vec->set_object_element_no_check_NORR(object, id_index);
		}
		
		return EidosValue_SP(vec);
	}
}

//	*********************	– (object<SLiMEidosBlock>)scriptBlocksWithIDs(integer ids)
//
EidosValue_SP Community::ExecuteMethod_scriptBlocksWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *ids_value = p_arguments[0].get();
	int ids_count = ids_value->Count();
	
	if (ids_count == 1)
	{
		// Singleton case
		slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(0, nullptr));
		SLiMEidosBlock *object = ScriptBlockWithID(id);
		
		if (!object)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_scriptBlocksWithIDs): scriptBlocksWithIDs() did not find a script block with id " << id << "." << EidosTerminate();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(object, gSLiM_SLiMEidosBlock_Class));
	}
	else
	{
		// Vector case
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_SLiMEidosBlock_Class))->resize_no_initialize_RR(ids_count);
		
		for (int id_index = 0; id_index < ids_count; id_index++)
		{
			slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(id_index, nullptr));
			SLiMEidosBlock *object = ScriptBlockWithID(id);
			
			if (!object)
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_scriptBlocksWithIDs): scriptBlocksWithIDs() did not find a script block with id " << id << "." << EidosTerminate();
			
			vec->set_object_element_no_check_NORR(object, id_index);
		}
		
		return EidosValue_SP(vec);
	}
}

//	*********************	– (object<Species>)speciesWithIDs(integer ids)
//
EidosValue_SP Community::ExecuteMethod_speciesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *ids_value = p_arguments[0].get();
	int ids_count = ids_value->Count();
	
	if (ids_count == 1)
	{
		// Singleton case
		slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(0, nullptr));
		Species *object = SpeciesWithID(id);
		
		if (!object)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_speciesWithIDs): speciesWithIDs() did not find a species with id " << id << "." << EidosTerminate();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(object, gSLiM_Species_Class));
	}
	else
	{
		// Vector case
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Species_Class))->resize_no_initialize_RR(ids_count);
		
		for (int id_index = 0; id_index < ids_count; id_index++)
		{
			slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(id_index, nullptr));
			Species *object = SpeciesWithID(id);
			
			if (!object)
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_speciesWithIDs): speciesWithIDs() did not find a species with id " << id << "." << EidosTerminate();
			
			vec->set_object_element_no_check_NORR(object, id_index);
		}
		
		return EidosValue_SP(vec);
	}
}

//	*********************	– (object<Subpopulation>)subpopulationsWithIDs(integer ids)
//
EidosValue_SP Community::ExecuteMethod_subpopulationsWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *ids_value = p_arguments[0].get();
	int ids_count = ids_value->Count();
	
	if (ids_count == 1)
	{
		// Singleton case
		slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(0, nullptr));
		Subpopulation *object = SubpopulationWithID(id);
		
		if (!object)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_subpopulationsWithIDs): subpopulationsWithIDs() did not find a subpopulation with id " << id << "." << EidosTerminate();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(object, gSLiM_Subpopulation_Class));
	}
	else
	{
		// Vector case
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Subpopulation_Class))->resize_no_initialize_RR(ids_count);
		
		for (int id_index = 0; id_index < ids_count; id_index++)
		{
			slim_objectid_t id = SLiMCastToObjectidTypeOrRaise(ids_value->IntAtIndex(id_index, nullptr));
			Subpopulation *object = SubpopulationWithID(id);
			
			if (!object)
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_subpopulationsWithIDs): subpopulationsWithIDs() did not find a subpopulation with id " << id << "." << EidosTerminate();
			
			vec->set_object_element_no_check_NORR(object, id_index);
		}
		
		return EidosValue_SP(vec);
	}
}

//	*********************	– (void)outputUsage(void)
//
EidosValue_SP Community::ExecuteMethod_outputUsage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// BEWARE: See also the -usage() method, which must be maintained in parallel with this
	std::ostream &out = p_interpreter.ExecutionOutputStream();
	
	// Save flags/precision and set to precision 1
	std::ios_base::fmtflags oldflags = out.flags();
	std::streamsize oldprecision = out.precision();
	out << std::fixed << std::setprecision(1);
	
	// Tally up usage across the simulation
	SLiMMemoryUsage_Community usage_community;
	SLiMMemoryUsage_Species usage_all_species;
	
	EIDOS_BZERO(&usage_all_species, sizeof(SLiMMemoryUsage_Species));
	
	TabulateSLiMMemoryUsage_Community(&usage_community, &p_interpreter.SymbolTable());
	
	for (Species *species : all_species_)
	{
		SLiMMemoryUsage_Species usage_one_species;
		
		species->TabulateSLiMMemoryUsage_Species(&usage_one_species);
		AccumulateMemoryUsageIntoTotal_Species(usage_one_species, usage_all_species);
	}
	
	// Print header
	out << "Memory usage summary:" << std::endl;
	
	// Chromosome
	out << "   Chromosome objects(" << usage_all_species.chromosomeObjects_count << "): " << PrintBytes(usage_all_species.chromosomeObjects) << std::endl;
	out << "      Mutation rate maps: " << PrintBytes(usage_all_species.chromosomeMutationRateMaps) << std::endl;
	out << "      Recombination rate maps: " << PrintBytes(usage_all_species.chromosomeRecombinationRateMaps) << std::endl;
	out << "      Ancestral nucleotides: " << PrintBytes(usage_all_species.chromosomeAncestralSequence) << std::endl;
	
	// Genome
	out << "   Genome objects (" << usage_all_species.genomeObjects_count << "): " << PrintBytes(usage_all_species.genomeObjects) << std::endl;
	out << "      External MutationRun* buffers: " << PrintBytes(usage_all_species.genomeExternalBuffers) << std::endl;
	out << "      Unused pool space: " << PrintBytes(usage_all_species.genomeUnusedPoolSpace) << std::endl;
	out << "      Unused pool buffers: " << PrintBytes(usage_all_species.genomeUnusedPoolBuffers) << std::endl;
	
	// GenomicElement
	out << "   GenomicElement objects (" << usage_all_species.genomicElementObjects_count << "): " << PrintBytes(usage_all_species.genomicElementObjects) << std::endl;
	
	// GenomicElementType
	out << "   GenomicElementType objects (" << usage_all_species.genomicElementTypeObjects_count << "): " << PrintBytes(usage_all_species.genomicElementTypeObjects) << std::endl;
	
	// Individual
	out << "   Individual objects (" << usage_all_species.individualObjects_count << "): " << PrintBytes(usage_all_species.individualObjects) << std::endl;
	out << "      Unused pool space: " << PrintBytes(usage_all_species.individualUnusedPoolSpace) << std::endl;
	
	// InteractionType
	out << "   InteractionType objects (" << usage_community.interactionTypeObjects_count << "): " << PrintBytes(usage_community.interactionTypeObjects) << std::endl;
	
	if (usage_community.interactionTypeObjects_count)
	{
		out << "      k-d trees: " << PrintBytes(usage_community.interactionTypeKDTrees) << std::endl;
		out << "      Position caches: " << PrintBytes(usage_community.interactionTypePositionCaches) << std::endl;
		out << "      Sparse vector pool: " << PrintBytes(usage_community.interactionTypeSparseVectorPool) << std::endl;
	}
	
	// Mutation
	out << "   Mutation objects (" << usage_all_species.mutationObjects_count << "): " << PrintBytes(usage_all_species.mutationObjects) << std::endl;
	out << "      Refcount buffer: " << PrintBytes(usage_community.mutationRefcountBuffer) << std::endl;
	out << "      Unused pool space: " << PrintBytes(usage_community.mutationUnusedPoolSpace) << std::endl;
	
	// MutationRun
	out << "   MutationRun objects (" << usage_all_species.mutationRunObjects_count << "): " << PrintBytes(usage_all_species.mutationRunObjects) << std::endl;
	out << "      External MutationIndex buffers: " << PrintBytes(usage_all_species.mutationRunExternalBuffers) << std::endl;
	out << "      Nonneutral mutation caches: " << PrintBytes(usage_all_species.mutationRunNonneutralCaches) << std::endl;
	out << "      Unused pool space: " << PrintBytes(usage_all_species.mutationRunUnusedPoolSpace) << std::endl;
	out << "      Unused pool buffers: " << PrintBytes(usage_all_species.mutationRunUnusedPoolBuffers) << std::endl;
	
	// MutationType
	out << "   MutationType objects (" << usage_all_species.mutationTypeObjects_count << "): " << PrintBytes(usage_all_species.mutationTypeObjects) << std::endl;
	
	// Species (including the Population object)
	out << "   Species objects: " << PrintBytes(usage_all_species.speciesObjects) << std::endl;
	out << "      Tree-sequence tables: " << PrintBytes(usage_all_species.speciesTreeSeqTables) << std::endl;
	
	// Subpopulation
	out << "   Subpopulation objects (" << usage_all_species.subpopulationObjects_count << "): " << PrintBytes(usage_all_species.subpopulationObjects) << std::endl;
	out << "      Fitness caches: " << PrintBytes(usage_all_species.subpopulationFitnessCaches) << std::endl;
	out << "      Parent tables: " << PrintBytes(usage_all_species.subpopulationParentTables) << std::endl;
	out << "      Spatial maps: " << PrintBytes(usage_all_species.subpopulationSpatialMaps) << std::endl;
	
	if (usage_all_species.subpopulationSpatialMapsDisplay)
		out << "      Spatial map display (SLiMgui): " << PrintBytes(usage_all_species.subpopulationSpatialMapsDisplay) << std::endl;
	
	// Substitution
	out << "   Substitution objects (" << usage_all_species.substitutionObjects_count << "): " << PrintBytes(usage_all_species.substitutionObjects) << std::endl;
	
	// Eidos usage
	out << "   Eidos: " << std::endl;
	out << "      EidosASTNode pool: " << PrintBytes(usage_community.eidosASTNodePool) << std::endl;
	out << "      EidosSymbolTable pool: " << PrintBytes(usage_community.eidosSymbolTablePool) << std::endl;
	out << "      EidosValue pool: " << PrintBytes(usage_community.eidosValuePool) << std::endl;
	out << "      File buffers: " << PrintBytes(usage_community.fileBuffers) << std::endl;
	
	out << "   # Total accounted for: " << PrintBytes(usage_community.totalMemoryUsage + usage_all_species.totalMemoryUsage) << std::endl;
	out << std::endl;
	
	// Restore flags/precision
	out.flags(oldflags);
	out.precision(oldprecision);
	
	return gStaticEidosValueVOID;
}

void Community::CheckScheduling(slim_tick_t p_target_tick, SLiMCycleStage p_target_stage)
{
	if (p_target_tick < tick_)
		EIDOS_TERMINATION << "ERROR (Community::CheckScheduling): event/callback scheduled for a past tick would not run." << EidosTerminate();
	if ((p_target_tick == tick_) && (p_target_stage < cycle_stage_))
		EIDOS_TERMINATION << "ERROR (Community::CheckScheduling): event/callback scheduled for the current tick, but for a past cycle stage, would not run." << EidosTerminate();
	if ((p_target_tick == tick_) && (p_target_stage == cycle_stage_))
		EIDOS_TERMINATION << "ERROR (Community::CheckScheduling): event/callback scheduled for the current tick, but for the currently executing cycle stage, would not run." << EidosTerminate();
}

//	*********************	– (object<SLiMEidosBlock>$)registerFirstEvent(Nis$ id, string$ source, [Ni$ start = NULL], [Ni$ end = NULL], [No<Species>$ ticksSpec = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerEarlyEvent(Nis$ id, string$ source, [Ni$ start = NULL], [Ni$ end = NULL], [No<Species>$ ticksSpec = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerLateEvent(Nis$ id, string$ source, [Ni$ start = NULL], [Ni$ end = NULL], [No<Species>$ ticksSpec = NULL])
//
EidosValue_SP Community::ExecuteMethod_registerFirstEarlyLateEvent(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *start_value = p_arguments[2].get();
	EidosValue *end_value = p_arguments[3].get();
	EidosValue *ticksSpec_value = p_arguments[4].get();
	
	slim_objectid_t script_id = -1;		// used if the id is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex(0, nullptr);
	slim_tick_t start_tick = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_tick_t end_tick = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_TICK + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	SLiMEidosBlockType target_type;
	
	if (p_method_id == gID_registerFirstEvent)
		target_type = SLiMEidosBlockType::SLiMEidosEventFirst;
	else if (p_method_id == gID_registerEarlyEvent)
		target_type = SLiMEidosBlockType::SLiMEidosEventEarly;
	else if (p_method_id == gID_registerLateEvent)
		target_type = SLiMEidosBlockType::SLiMEidosEventLate;
	else
		EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_registerFirstEarlyLateEvent): (internal error) unrecognized p_method_id." << EidosTerminate();
	
	if (start_tick > end_tick)
		EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_registerFirstEarlyLateEvent): register" << ((p_method_id == gID_registerFirstEvent) ? "First" : ((p_method_id == gID_registerEarlyEvent) ? "Early" : "Late")) << "Event() requires start <= end." << EidosTerminate();
	
	SLiMCycleStage target_stage;
	
	if (target_type == SLiMEidosBlockType::SLiMEidosEventFirst)
		target_stage = (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage0ExecuteFirstScripts : SLiMCycleStage::kNonWFStage0ExecuteFirstScripts;
	else if (target_type == SLiMEidosBlockType::SLiMEidosEventEarly)
		target_stage = (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage1ExecuteEarlyScripts : SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts;
	else if (target_type == SLiMEidosBlockType::SLiMEidosEventLate)
		target_stage = (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage5ExecuteLateScripts : SLiMCycleStage::kNonWFStage6ExecuteLateScripts;
	else
		EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_registerFirstEarlyLateEvent): (internal error) unrecognized target_type." << EidosTerminate();
	
	Species *ticksSpec = ((ticksSpec_value->Type() != EidosValueType::kValueNULL) ? (Species *)ticksSpec_value->ObjectElementAtIndex(0, nullptr) : nullptr);
	
	if (ticksSpec && !is_explicit_species_)
		EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_registerFirstEarlyLateEvent): ticksSpec must be NULL in models without explicit species declarations." << EidosTerminate();
	
	CheckScheduling(start_tick, target_stage);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, -1, target_type, start_tick, end_tick, nullptr, ticksSpec);
	
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerInteractionCallback(Nis$ id, string$ source, io<InteractionType>$ intType, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP Community::ExecuteMethod_registerInteractionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *intType_value = p_arguments[2].get();
	EidosValue *subpop_value = p_arguments[3].get();
	EidosValue *start_value = p_arguments[4].get();
	EidosValue *end_value = p_arguments[5].get();
	
	slim_objectid_t script_id = -1;		// used if the id is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex(0, nullptr);
	slim_objectid_t int_type_id = (intType_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(intType_value->IntAtIndex(0, nullptr)) : ((InteractionType *)intType_value->ObjectElementAtIndex(0, nullptr))->interaction_type_id_;
	slim_objectid_t subpop_id = -1;
	slim_tick_t start_tick = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_tick_t end_tick = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_TICK + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
	
	if (start_tick > end_tick)
		EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_registerInteractionCallback): registerInteractionCallback() requires start <= end." << EidosTerminate();
	
	CheckScheduling(start_tick, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage7AdvanceTickCounter : SLiMCycleStage::kNonWFStage7AdvanceTickCounter);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, -1, SLiMEidosBlockType::SLiMEidosInteractionCallback, start_tick, end_tick, nullptr, nullptr);
	
	new_script_block->interaction_type_id_ = int_type_id;
	new_script_block->subpopulation_id_ = subpop_id;
	
	// SPECIES CONSISTENCY CHECK (done by AddScriptBlock())
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>)rescheduleScriptBlock(io<SLiMEidosBlock>$ block, [Ni$ start = NULL], [Ni$ end = NULL], [Ni ticks = NULL])
//
EidosValue_SP Community::ExecuteMethod_rescheduleScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *block_value = (EidosValue_Object *)p_arguments[0].get();
	EidosValue *start_value = p_arguments[1].get();
	EidosValue *end_value = p_arguments[2].get();
	EidosValue *ticks_value = p_arguments[3].get();
	
	SLiMEidosBlock *block = SLiM_ExtractSLiMEidosBlockFromEidosValue_io(block_value, 0, this, nullptr, "rescheduleScriptBlock()");	// agnostic to species
	bool start_null = (start_value->Type() == EidosValueType::kValueNULL);
	bool end_null = (end_value->Type() == EidosValueType::kValueNULL);
	bool ticks_null = (ticks_value->Type() == EidosValueType::kValueNULL);
	
	if (block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
	{
		// this should never be hit, because the user should have no way to get a reference to a function block
		EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_rescheduleScriptBlock): (internal error) rescheduleScriptBlock() cannot be called on user-defined function script blocks." << EidosTerminate();
	}
	
	// Figure out what cycle stage the rescheduled block executes in; this is annoying, but necessary for the new scheduling check call
	SLiMCycleStage stage = SLiMCycleStage::kStagePostCycle;	// unused below, just here to silence a warning
	
	// NOLINTBEGIN(*-branch-clone) : multiple internal tick stages map to the same user-level stage
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		switch (block->type_)
		{
			case SLiMEidosBlockType::SLiMEidosEventFirst:				stage = SLiMCycleStage::kWFStage0ExecuteFirstScripts; break;
			case SLiMEidosBlockType::SLiMEidosEventEarly:				stage = SLiMCycleStage::kWFStage1ExecuteEarlyScripts; break;
			case SLiMEidosBlockType::SLiMEidosEventLate:				stage = SLiMCycleStage::kWFStage5ExecuteLateScripts; break;
			case SLiMEidosBlockType::SLiMEidosInitializeCallback:		stage = SLiMCycleStage::kStagePreCycle; break;
			case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	stage = SLiMCycleStage::kWFStage6CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	stage = SLiMCycleStage::kWFStage6CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosInteractionCallback:		stage = SLiMCycleStage::kWFStage7AdvanceTickCounter; break;
			case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		stage = SLiMCycleStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		stage = SLiMCycleStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	stage = SLiMCycleStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosMutationCallback:			stage = SLiMCycleStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			stage = SLiMCycleStage::kWFStage4SwapGenerations; break;				// never hit
			case SLiMEidosBlockType::SLiMEidosReproductionCallback:		stage = SLiMCycleStage::kWFStage2GenerateOffspring; break;				// never hit
			case SLiMEidosBlockType::SLiMEidosNoBlockType:
			case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_rescheduleScriptBlock): (internal error) rescheduleScriptBlock() cannot be called on this type of script block." << EidosTerminate();
		}
	}
	else
	{
		switch (block->type_)
		{
			case SLiMEidosBlockType::SLiMEidosEventFirst:				stage = SLiMCycleStage::kNonWFStage0ExecuteFirstScripts; break;
			case SLiMEidosBlockType::SLiMEidosEventEarly:				stage = SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts; break;
			case SLiMEidosBlockType::SLiMEidosEventLate:				stage = SLiMCycleStage::kNonWFStage6ExecuteLateScripts; break;
			case SLiMEidosBlockType::SLiMEidosInitializeCallback:		stage = SLiMCycleStage::kStagePreCycle; break;
			case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:	stage = SLiMCycleStage::kNonWFStage3CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:	stage = SLiMCycleStage::kNonWFStage3CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosInteractionCallback:		stage = SLiMCycleStage::kNonWFStage7AdvanceTickCounter; break;
			case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		stage = SLiMCycleStage::kNonWFStage1GenerateOffspring; break;			// never hit
			case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		stage = SLiMCycleStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	stage = SLiMCycleStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosMutationCallback:			stage = SLiMCycleStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosSurvivalCallback:			stage = SLiMCycleStage::kNonWFStage4SurvivalSelection; break;
			case SLiMEidosBlockType::SLiMEidosReproductionCallback:		stage = SLiMCycleStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosNoBlockType:
			case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_rescheduleScriptBlock): (internal error) rescheduleScriptBlock() cannot be called on this type of script block." << EidosTerminate();
		}
	}
	// NOLINTEND(*-branch-clone)
	
	if ((!start_null || !end_null) && ticks_null)
	{
		// start/end case; this is simple
		
		slim_tick_t start = (start_null ? 1 : SLiMCastToTickTypeOrRaise(start_value->IntAtIndex(0, nullptr)));
		slim_tick_t end = (end_null ? SLIM_MAX_TICK + 1 : SLiMCastToTickTypeOrRaise(end_value->IntAtIndex(0, nullptr)));
		
		if (start > end)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_rescheduleScriptBlock): rescheduleScriptBlock() requires start <= end." << EidosTerminate();
		
		CheckScheduling(start, stage);
		
		block->start_tick_ = start;
		block->end_tick_ = end;
		last_script_block_tick_cached_ = false;
		script_block_types_cached_ = false;
		scripts_changed_ = true;
		
#ifdef SLIMGUI
		gSLiMScheduling << "\t\trescheduleScriptBlock() called (with start " << start << ", end " << end << ") for block: ";
		block->PrintDeclaration(gSLiMScheduling, this);
		gSLiMScheduling << std::endl;
#endif
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(block, gSLiM_SLiMEidosBlock_Class));
	}
	else if (!ticks_null && (start_null && end_null))
	{
		// ticks case; this is complicated
		
		// first, fetch the ticks and make sure they are in bounds
		std::vector<slim_tick_t> ticks;
		int tick_count = ticks_value->Count();
		
		if (tick_count < 1)
			EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_rescheduleScriptBlock): rescheduleScriptBlock() requires at least one tick; use deregisterScriptBlock() to remove a script block from the simulation." << EidosTerminate();
		
		ticks.reserve(tick_count);
		
		for (int tick_index = 0; tick_index < tick_count; ++tick_index)
			ticks.emplace_back(SLiMCastToTickTypeOrRaise(ticks_value->IntAtIndex(tick_index, nullptr)));
		
		// next, sort the tick list and check that the first scheduling it requests is not in the past
		std::sort(ticks.begin(), ticks.end());
		
		CheckScheduling(ticks.front(), stage);
		
		// finally, go through the tick vector and schedule blocks for sequential runs
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_SLiMEidosBlock_Class));
		EidosValue_SP result_SP = EidosValue_SP(vec);
		bool first_block = true;
		
		slim_tick_t start = -10;
		slim_tick_t end = -10;
		int tick_index = 0;
		
		// I'm sure there's a prettier algorithm for finding the sequential runs, but I'm not seeing it right now.
		// The tricky thing is that I want there to be only a single place in the code where a block is scheduled;
		// it seems easy to write a version where blocks get scheduled in two places, a main case and a tail case.
		while (true)
		{
			slim_tick_t tick = ticks[tick_index];
			bool reached_end_in_seq = false;
			
			if (tick == end + 1)			// sequential value seen; move on to the next sequential value
			{
				end++;
				
				if (++tick_index < tick_count)
					continue;
				reached_end_in_seq = true;
			}
			else if (tick <= end)
			{
				EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_rescheduleScriptBlock): rescheduleScriptBlock() requires that the tick vector contain unique values; the same tick cannot be used twice." << EidosTerminate();
			}
			
			// make new block and move on to start the next sequence
		makeBlock:
			if ((start != -10) && (end != -10))
			{
				// start and end define the range of the block to schedule; first_block
				// determines whether we use the existing block or make a new one
				if (first_block)
				{
					block->start_tick_ = start;
					block->end_tick_ = end;
					first_block = false;
					last_script_block_tick_cached_ = false;
					script_block_types_cached_ = false;
					scripts_changed_ = true;
					
					vec->push_object_element_NORR(block);
				}
				else
				{
					SLiMEidosBlock *new_script_block = new SLiMEidosBlock(-1, block->compound_statement_node_->token_->token_string_, block->user_script_line_offset_, block->type_, start, end, block->species_spec_, block->ticks_spec_);
					
					AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
					
					vec->push_object_element_NORR(new_script_block);
				}
			}
			
			start = tick;
			end = tick;
			++tick_index;
			
			if ((tick_index == tick_count) && !reached_end_in_seq)
				goto makeBlock;
			else if (tick_index >= tick_count)
				break;
		}
		
#ifdef SLIMGUI
		gSLiMScheduling << "\t\trescheduleScriptBlock() called (with a ticks schedule) for block: ";
		block->PrintDeclaration(gSLiMScheduling, this);
		gSLiMScheduling << std::endl;
#endif
		
		return result_SP;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Community::ExecuteMethod_rescheduleScriptBlock): rescheduleScriptBlock() requires that either start/end or ticks be supplied, but not both." << EidosTerminate();
	}
}

//	*********************	- (void)simulationFinished(void)
//
EidosValue_SP Community::ExecuteMethod_simulationFinished(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	// Note that Species::ExecuteMethod_simulationFinished() calls this method to forward simulationFinished() on to us!
	// This means that we need to have an identical Eidos method signature, etc., so the forwarding goes smoothly.
	
#ifdef SLIMGUI
	gSLiMScheduling << "\t\tsimulationFinished() called" << std::endl;
#endif
	
	sim_declared_finished_ = true;
	
	return gStaticEidosValueVOID;
}

//	*********************	– (float$)usage(void)
//
EidosValue_SP Community::ExecuteMethod_usage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// BEWARE: See also the -outputUsage() method, which must be maintained in parallel with this
	
	// Tally up usage across the simulation
	SLiMMemoryUsage_Community usage_community;
	SLiMMemoryUsage_Species usage_all_species;
	
	EIDOS_BZERO(&usage_all_species, sizeof(SLiMMemoryUsage_Species));
	
	TabulateSLiMMemoryUsage_Community(&usage_community, &p_interpreter.SymbolTable());
	
	for (Species *species : all_species_)
	{
		SLiMMemoryUsage_Species usage_one_species;
		
		species->TabulateSLiMMemoryUsage_Species(&usage_one_species);
		AccumulateMemoryUsageIntoTotal_Species(usage_one_species, usage_all_species);
	}
	
	size_t usage = usage_community.totalMemoryUsage + usage_all_species.totalMemoryUsage;
	double usage_MB = usage / (1024.0 * 1024.0);
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(usage_MB));
	
	return result_SP;
}


//
//	Community_Class
//
#pragma mark -
#pragma mark Community_Class
#pragma mark -

EidosClass *gSLiM_Community_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Community_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Community_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_allGenomicElementTypes,	true,	kEidosValueMaskObject, gSLiM_GenomicElementType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_allInteractionTypes,	true,	kEidosValueMaskObject, gSLiM_InteractionType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_allMutationTypes,		true,	kEidosValueMaskObject, gSLiM_MutationType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_allScriptBlocks,		true,	kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_allSpecies,				true,	kEidosValueMaskObject, gSLiM_Species_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_allSubpopulations,		true,	kEidosValueMaskObject, gSLiM_Subpopulation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_logFiles,				true,	kEidosValueMaskObject, gSLiM_LogFile_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_modelType,				true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tick,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_cycleStage,				true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_verbosity,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Community_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Community_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_createLogFile, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_LogFile_Class))->AddString_S(gEidosStr_filePath)->AddString_ON("initialContents", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("compress", gStaticEidosValue_LogicalF)->AddString_OS("sep", gStaticEidosValue_StringComma)->AddInt_OSN("logInterval", gStaticEidosValueNULL)->AddInt_OSN("flushInterval", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_deregisterScriptBlock, kEidosValueMaskVOID))->AddIntObject("scriptBlocks", gSLiM_SLiMEidosBlock_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_genomicElementTypesWithIDs, kEidosValueMaskObject, gSLiM_GenomicElementType_Class))->AddInt("ids"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interactionTypesWithIDs, kEidosValueMaskObject, gSLiM_InteractionType_Class))->AddInt("ids"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationTypesWithIDs, kEidosValueMaskObject, gSLiM_MutationType_Class))->AddInt("ids"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputUsage, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerFirstEvent, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL)->AddObject_OSN("ticksSpec", gSLiM_Species_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerEarlyEvent, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL)->AddObject_OSN("ticksSpec", gSLiM_Species_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerLateEvent, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL)->AddObject_OSN("ticksSpec", gSLiM_Species_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerInteractionCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_S("intType", gSLiM_InteractionType_Class)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_rescheduleScriptBlock, kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class))->AddIntObject_S("block", gSLiM_SLiMEidosBlock_Class)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL)->AddInt_ON("ticks", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_scriptBlocksWithIDs, kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class))->AddInt("ids"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_simulationFinished, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_speciesWithIDs, kEidosValueMaskObject, gSLiM_Species_Class))->AddInt("ids"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_subpopulationsWithIDs, kEidosValueMaskObject, gSLiM_Subpopulation_Class))->AddInt("ids"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_usage, kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
