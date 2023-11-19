//
//  slim_globals.cpp
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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


#include "slim_globals.h"

#include "chromosome.h"
#include "individual.h"
#include "interaction_type.h"
#include "genome.h"
#include "genomic_element.h"
#include "genomic_element_type.h"
#include "log_file.h"
#include "mutation.h"
#include "mutation_type.h"
#include "slim_eidos_block.h"
#include "community.h"
#include "spatial_map.h"
#include "species.h"
#include "substitution.h"
#include "subpopulation.h"

#include "mutation_run.h"

#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#include <algorithm>

#include "json.hpp"


// Require 64-bit; apparently there are some issues on 32-bit, and nobody should be doing that anyway
static_assert(sizeof(char *) == 8, "SLiM must be built for 64-bit, not 32-bit.");


EidosValue_String_SP gStaticEidosValue_StringA;
EidosValue_String_SP gStaticEidosValue_StringC;
EidosValue_String_SP gStaticEidosValue_StringG;
EidosValue_String_SP gStaticEidosValue_StringT;


void SLiM_WarmUp(void)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("SLiM_WarmUp(): illegal when parallel");
	
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		// Create the global class objects for all SLiM Eidos classes, from superclass to subclass
		// This breaks encapsulation, kind of, but it needs to be done here, in order, so that superclass objects exist,
		// and so that the global string names for the classes have already been set up by C++'s static initialization
		gSLiM_Chromosome_Class =			new Chromosome_Class(			gStr_Chromosome,			gEidosDictionaryRetained_Class);
		gSLiM_Individual_Class =			new Individual_Class(			gEidosStr_Individual,		gEidosDictionaryUnretained_Class);
		gSLiM_InteractionType_Class =		new InteractionType_Class(		gStr_InteractionType,		gEidosDictionaryUnretained_Class);
		gSLiM_Genome_Class =				new Genome_Class(				gEidosStr_Genome,			gEidosObject_Class);
		gSLiM_GenomicElement_Class =		new GenomicElement_Class(		gStr_GenomicElement,		gEidosObject_Class);
		gSLiM_GenomicElementType_Class =	new GenomicElementType_Class(	gStr_GenomicElementType,	gEidosDictionaryUnretained_Class);
		gSLiM_LogFile_Class =				new LogFile_Class(				gStr_LogFile,				gEidosDictionaryRetained_Class);
		gSLiM_Mutation_Class =				new Mutation_Class(				gEidosStr_Mutation,			gEidosDictionaryRetained_Class);
		gSLiM_MutationType_Class =			new MutationType_Class(			gStr_MutationType,			gEidosDictionaryUnretained_Class);
		gSLiM_SLiMEidosBlock_Class =		new SLiMEidosBlock_Class(		gStr_SLiMEidosBlock,		gEidosDictionaryUnretained_Class);
		gSLiM_Community_Class =				new Community_Class(			gStr_Community,				gEidosDictionaryUnretained_Class);
		gSLiM_SpatialMap_Class =			new SpatialMap_Class(			gStr_SpatialMap,			gEidosDictionaryRetained_Class);
		gSLiM_Species_Class =				new Species_Class(				gStr_Species,				gEidosDictionaryUnretained_Class);
		gSLiM_Substitution_Class =			new Substitution_Class(			gStr_Substitution,			gEidosDictionaryRetained_Class);
		gSLiM_Subpopulation_Class =			new Subpopulation_Class(		gStr_Subpopulation,			gEidosDictionaryUnretained_Class);
		
		// Tell all registered classes to initialize their dispatch tables; doing this here saves a flag check later
		// Note that this can't be done in the EidosClass constructor because the vtable is not set up for the subclass yet
		for (EidosClass *eidos_class : EidosClass::RegisteredClasses(true, true))
			eidos_class->CacheDispatchTables();
		
		// Set up our shared pool for Mutation objects
		SLiM_CreateMutationBlock();
		
		// Configure the Eidos context information
		SLiM_ConfigureContext();
		
		// Allocate global permanents
		gStaticEidosValue_StringA = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_A));
		gStaticEidosValue_StringC = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_C));
		gStaticEidosValue_StringG = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_G));
		gStaticEidosValue_StringT = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_T));
		
#if DO_MEMORY_CHECKS
		// Check for a memory limit and prepare for memory-limit testing
		Eidos_CheckRSSAgainstMax("SLiM_WarmUp()", "This internal check should never fail!");
#endif
		
		//std::cout << "sizeof(Individual) == " << sizeof(Individual) << std::endl;
		//std::cout << "sizeof(Mutation) == " << sizeof(Mutation) << std::endl;
		
		//std::cout << "sizeof(int) == " << sizeof(int) << std::endl;
		//std::cout << "sizeof(long) == " << sizeof(long) << std::endl;
		//std::cout << "sizeof(size_t) == " << sizeof(size_t) << std::endl;
		
		// Test that our tskit metadata schemas are valid JSON, and print them out formatted for debugging purposes if desired
		nlohmann::json top_level_schema, edge_schema, site_schema, mutation_schema, node_schema, individual_schema, population_schema;
		
		try {
			top_level_schema = nlohmann::json::parse(gSLiM_tsk_metadata_schema);
		}  catch (...) {
			EIDOS_TERMINATION << "ERROR (SLiM_WarmUp): (internal error) gSLiM_tsk_metadata_schema must be a JSON string." << EidosTerminate();
		}
		try {
			if (gSLiM_tsk_edge_metadata_schema.length())
				edge_schema = nlohmann::json::parse(gSLiM_tsk_edge_metadata_schema);
		}  catch (...) {
			EIDOS_TERMINATION << "ERROR (SLiM_WarmUp): (internal error) gSLiM_tsk_edge_metadata_schema must be a JSON string." << EidosTerminate();
		}
		try {
			if (gSLiM_tsk_site_metadata_schema.length())
				site_schema = nlohmann::json::parse(gSLiM_tsk_site_metadata_schema);
		}  catch (...) {
			EIDOS_TERMINATION << "ERROR (SLiM_WarmUp): (internal error) gSLiM_tsk_site_metadata_schema must be a JSON string." << EidosTerminate();
		}
		try {
			mutation_schema = nlohmann::json::parse(gSLiM_tsk_mutation_metadata_schema);
		}  catch (...) {
			EIDOS_TERMINATION << "ERROR (SLiM_WarmUp): (internal error) gSLiM_tsk_mutation_metadata_schema must be a JSON string." << EidosTerminate();
		}
		try {
			node_schema = nlohmann::json::parse(gSLiM_tsk_node_metadata_schema);
		}  catch (...) {
			EIDOS_TERMINATION << "ERROR (SLiM_WarmUp): (internal error) gSLiM_tsk_node_metadata_schema must be a JSON string." << EidosTerminate();
		}
		try {
			individual_schema = nlohmann::json::parse(gSLiM_tsk_individual_metadata_schema);
		}  catch (...) {
			EIDOS_TERMINATION << "ERROR (SLiM_WarmUp): (internal error) gSLiM_tsk_individual_metadata_schema must be a JSON string." << EidosTerminate();
		}
		try {
			population_schema = nlohmann::json::parse(gSLiM_tsk_population_metadata_schema);
		}  catch (...) {
			EIDOS_TERMINATION << "ERROR (SLiM_WarmUp): (internal error) gSLiM_tsk_population_metadata_schema must be a JSON string." << EidosTerminate();
		}
		
#if 0
#warning printing of JSON schemas should be disabled in a production build
		std::cout << "gSLiM_tsk_metadata_schema == " << std::endl << top_level_schema.dump(4) << std::endl << std::endl;
		std::cout << "gSLiM_tsk_edge_metadata_schema == " << std::endl << edge_schema.dump(4) << std::endl << std::endl;
		std::cout << "gSLiM_tsk_site_metadata_schema == " << std::endl << site_schema.dump(4) << std::endl << std::endl;
		std::cout << "gSLiM_tsk_mutation_metadata_schema == " << std::endl << mutation_schema.dump(4) << std::endl << std::endl;
		std::cout << "gSLiM_tsk_node_metadata_schema == " << std::endl << node_schema.dump(4) << std::endl << std::endl;
		std::cout << "gSLiM_tsk_individual_metadata_schema == " << std::endl << individual_schema.dump(4) << std::endl << std::endl;
		std::cout << "gSLiM_tsk_population_metadata_schema == " << std::endl << population_schema.dump(4) << std::endl << std::endl;
#endif
	}
}


// ostringstreams for SLiM output; see the header for details
std::ostringstream gSLiMOut;
std::ostringstream gSLiMError;

#ifdef SLIMGUI
std::ostringstream gSLiMScheduling;
#endif


#pragma mark -
#pragma mark Types and max values
#pragma mark -

// Functions for casting from Eidos ints (int64_t) to SLiM int types safely
void SLiM_RaiseTickRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaiseTickRangeError): value " << p_long_value << " for a tick index or duration is out of range." << EidosTerminate();
}

void SLiM_RaiseAgeRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaiseAgeRangeError): value " << p_long_value << " for an individual age is out of range." << EidosTerminate();
}

void SLiM_RaisePositionRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaisePositionRangeError): value " << p_long_value << " for a chromosome position or length is out of range." << EidosTerminate();
}

void SLiM_RaisePedigreeIDRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaisePedigreeIDRangeError): value " << p_long_value << " for an individual pedigree ID is out of range." << EidosTerminate();
}

void SLiM_RaiseObjectidRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaiseObjectidRangeError): value " << p_long_value << " for a SLiM object identifier value is out of range." << EidosTerminate();
}

void SLiM_RaisePopsizeRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaisePopsizeRangeError): value " << p_long_value << " for a subpopulation size, individual index, or genome index is out of range." << EidosTerminate();
}

void SLiM_RaiseUsertagRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaiseUsertagRangeError): value " << p_long_value << " for a user-supplied tag is out of range." << EidosTerminate();
}

void SLiM_RaisePolymorphismidRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaisePolymorphismidRangeError): value " << p_long_value << " for a polymorphism identifier is out of range." << EidosTerminate();
}

Community &SLiM_GetCommunityFromInterpreter(EidosInterpreter &p_interpreter)
{
#if DEBUG
	// Use dynamic_cast<> only in DEBUG since it is hella slow
	Community *community = dynamic_cast<Community *>(p_interpreter.Context());
#else
	Community *community = (Community *)(p_interpreter.Context());
#endif
	
	if (!community)
		EIDOS_TERMINATION << "ERROR (SLiM_GetCommunityFromInterpreter): (internal error) the community is not registered as the context pointer." << EidosTerminate();
	
	return *community;
}

slim_objectid_t SLiM_ExtractObjectIDFromEidosValue_is(EidosValue *p_value, int p_index, char p_prefix_char)
{
	return (p_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(p_value->StringAtIndex(p_index, nullptr), p_prefix_char, nullptr);
}

MutationType *SLiM_ExtractMutationTypeFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name)
{
	MutationType *found_muttype = nullptr;
	
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		
		if (p_species)
		{
			// Look in the species, if one was supplied
			found_muttype = p_species->MutationTypeWithID(mutation_type_id);
			
			if (!found_muttype)
				EIDOS_TERMINATION << "ERROR (SLiM_ExtractMutationTypeFromEidosValue_io): " << p_method_name << " mutation type m" << mutation_type_id << " not defined in the focal species." << EidosTerminate();
		}
		else
		{
			// Otherwise, look in all species in the community
			for (Species *species : p_community->AllSpecies())
			{
				found_muttype = species->MutationTypeWithID(mutation_type_id);
				
				if (found_muttype)
					break;
			}
			
			if (!found_muttype)
				EIDOS_TERMINATION << "ERROR (SLiM_ExtractMutationTypeFromEidosValue_io): " << p_method_name << " mutation type m" << mutation_type_id << " not defined." << EidosTerminate();
		}
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		found_muttype = dynamic_cast<MutationType *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		found_muttype = (MutationType *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
		
		if (!found_muttype)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractMutationTypeFromEidosValue_io): (internal error) " << p_method_name << " was passed an object that is not a mutation type." << EidosTerminate();
		
		if (p_species && (&found_muttype->species_ != p_species))
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractMutationTypeFromEidosValue_io): " << p_method_name << " mutation type m" << found_muttype->mutation_type_id_ << " not defined in the focal species." << EidosTerminate();
	}
	
	return found_muttype;
}

GenomicElementType *SLiM_ExtractGenomicElementTypeFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name)
{
	GenomicElementType *found_getype = nullptr;
	
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t getype_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		
		if (p_species)
		{
			// Look in the species, if one was supplied
			found_getype = p_species->GenomicElementTypeWithID(getype_id);
			
			if (!found_getype)
				EIDOS_TERMINATION << "ERROR (SLiM_ExtractGenomicElementTypeFromEidosValue_io): " << p_method_name << " genomic element type g" << getype_id << " not defined in the focal species." << EidosTerminate();
		}
		else
		{
			// Otherwise, look in all species in the community
			for (Species *species : p_community->AllSpecies())
			{
				found_getype = species->GenomicElementTypeWithID(getype_id);
				
				if (found_getype)
					break;
			}
			
			if (!found_getype)
				EIDOS_TERMINATION << "ERROR (SLiM_ExtractGenomicElementTypeFromEidosValue_io): " << p_method_name << " genomic element type g" << getype_id << " not defined." << EidosTerminate();
		}
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		found_getype = dynamic_cast<GenomicElementType *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		found_getype = (GenomicElementType *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
		
		if (!found_getype)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractGenomicElementTypeFromEidosValue_io): (internal error) " << p_method_name << " was passed an object that is not a genomic element type." << EidosTerminate();
		
		if (p_species && (&found_getype->species_ != p_species))
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractGenomicElementTypeFromEidosValue_io): " << p_method_name << " genomic element type g" << found_getype->genomic_element_type_id_ << " not defined in the focal species." << EidosTerminate();
	}
	
	return found_getype;
}

Subpopulation *SLiM_ExtractSubpopulationFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name)
{
	Subpopulation *found_subpop = nullptr;
	
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t source_subpop_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		
		if (p_species)
		{
			// Look in the species, if one was supplied
			found_subpop = p_species->SubpopulationWithID(source_subpop_id);
			
			if (!found_subpop)
				EIDOS_TERMINATION << "ERROR (SLiM_ExtractSubpopulationFromEidosValue_io): " << p_method_name << " subpopulation p" << source_subpop_id << " not defined in the focal species." << EidosTerminate();
		}
		else
		{
			// Otherwise, look in all species in the community
			for (Species *species : p_community->AllSpecies())
			{
				found_subpop = species->SubpopulationWithID(source_subpop_id);
				
				if (found_subpop)
					break;
			}
			
			if (!found_subpop)
				EIDOS_TERMINATION << "ERROR (SLiM_ExtractSubpopulationFromEidosValue_io): " << p_method_name << " subpopulation p" << source_subpop_id << " not defined." << EidosTerminate();
		}
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		found_subpop = dynamic_cast<Subpopulation *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		found_subpop = (Subpopulation *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
		
		if (!found_subpop)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSubpopulationFromEidosValue_io): (internal error) " << p_method_name << " was passed an object that is not a subpopulation." << EidosTerminate();
		
		if (p_species && (&found_subpop->species_ != p_species))
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSubpopulationFromEidosValue_io): " << p_method_name << " subpopulation p" << found_subpop->subpopulation_id_ << " not defined in the focal species." << EidosTerminate();
	}
	
	return found_subpop;
}

SLiMEidosBlock *SLiM_ExtractSLiMEidosBlockFromEidosValue_io(EidosValue *p_value, int p_index, Community *p_community, Species *p_species, const char *p_method_name)
{
	SLiMEidosBlock *found_block = nullptr;
	
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t block_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		std::vector<SLiMEidosBlock*> &script_blocks = p_community->AllScriptBlocks();
		
		for (SLiMEidosBlock *temp_found_block : script_blocks)
			if (temp_found_block->block_id_ == block_id)
			{
				found_block = temp_found_block;
				break;
			}
		
		if (!found_block)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSLiMEidosBlockFromEidosValue_io): " << p_method_name << " SLiMEidosBlock s" << block_id << " not defined." << EidosTerminate();
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		found_block = dynamic_cast<SLiMEidosBlock *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		found_block = (SLiMEidosBlock *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
		
		if (!found_block)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSLiMEidosBlockFromEidosValue_io): (internal error) " << p_method_name << " was passed an object that is not a SLiMEidosBlock." << EidosTerminate();
		
	}
	
	if (p_species && (found_block->species_spec_ != p_species))
		EIDOS_TERMINATION << "ERROR (SLiM_ExtractSLiMEidosBlockFromEidosValue_io): " << p_method_name << " SLiMEidosBlock s" << found_block->block_id_ << " not defined in the focal species." << EidosTerminate();
	
	return found_block;
}

Species *SLiM_ExtractSpeciesFromEidosValue_No(EidosValue *p_value, int p_index, Community *p_community, const char *p_method_name)
{
	Species *found_species = nullptr;
	
	if (p_value->Type() == EidosValueType::kValueNULL)
	{
		const std::vector<Species *> &all_species = p_community->AllSpecies();
		
		if (all_species.size() == 1)
			found_species = all_species[0];
		else
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSpeciesFromEidosValue_No): " << p_method_name << " requires a species to be supplied in multispecies models." << EidosTerminate();
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		found_species = dynamic_cast<Species *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		found_species = (Species *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
		
		if (!found_species)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSpeciesFromEidosValue_No): (internal error) " << p_method_name << " was passed an object that is not a Species." << EidosTerminate();
	}
	
	return found_species;
}


#pragma mark -
#pragma mark Memory management
#pragma mark -

void SumUpMemoryUsage_Species(SLiMMemoryUsage_Species &p_usage)
{
	p_usage.totalMemoryUsage =
		p_usage.chromosomeObjects +
		p_usage.chromosomeMutationRateMaps +
		p_usage.chromosomeRecombinationRateMaps +
		p_usage.chromosomeAncestralSequence +
		p_usage.genomeObjects +
		p_usage.genomeExternalBuffers +
		p_usage.genomeUnusedPoolSpace +
		p_usage.genomeUnusedPoolBuffers +
		p_usage.genomicElementObjects +
		p_usage.genomicElementTypeObjects +
		p_usage.individualObjects +
		p_usage.individualUnusedPoolSpace +
		p_usage.mutationObjects +
		p_usage.mutationRunObjects +
		p_usage.mutationRunExternalBuffers +
		p_usage.mutationRunNonneutralCaches +
		p_usage.mutationRunUnusedPoolSpace +
		p_usage.mutationRunUnusedPoolBuffers +
		p_usage.mutationTypeObjects +
		p_usage.speciesObjects +
		p_usage.speciesTreeSeqTables +
		p_usage.subpopulationObjects +
		p_usage.subpopulationFitnessCaches +
		p_usage.subpopulationParentTables +
		p_usage.subpopulationSpatialMaps +
		p_usage.subpopulationSpatialMapsDisplay +
		p_usage.substitutionObjects;
}

void SumUpMemoryUsage_Community(SLiMMemoryUsage_Community &p_usage)
{
	p_usage.totalMemoryUsage =
		p_usage.communityObjects +
		p_usage.mutationRefcountBuffer +
		p_usage.mutationUnusedPoolSpace +
		p_usage.interactionTypeObjects +
		p_usage.interactionTypeKDTrees +
		p_usage.interactionTypePositionCaches +
		p_usage.interactionTypeSparseVectorPool +
		p_usage.eidosASTNodePool +
		p_usage.eidosSymbolTablePool +
		p_usage.eidosValuePool + 
		p_usage.fileBuffers;
}

void AccumulateMemoryUsageIntoTotal_Species(SLiMMemoryUsage_Species &p_usage, SLiMMemoryUsage_Species &p_total)
{
	// p_total += p_usage;
	
	p_total.chromosomeObjects_count += p_usage.chromosomeObjects_count;
	p_total.chromosomeObjects += p_usage.chromosomeObjects;
	p_total.chromosomeMutationRateMaps += p_usage.chromosomeMutationRateMaps;
	p_total.chromosomeRecombinationRateMaps += p_usage.chromosomeRecombinationRateMaps;
	p_total.chromosomeAncestralSequence += p_usage.chromosomeAncestralSequence;
	
	p_total.genomeObjects_count += p_usage.genomeObjects_count;
	p_total.genomeObjects += p_usage.genomeObjects;
	p_total.genomeExternalBuffers += p_usage.genomeExternalBuffers;
	p_total.genomeUnusedPoolSpace += p_usage.genomeUnusedPoolSpace;
	p_total.genomeUnusedPoolBuffers += p_usage.genomeUnusedPoolBuffers;
	
	p_total.genomicElementObjects_count += p_usage.genomicElementObjects_count;
	p_total.genomicElementObjects += p_usage.genomicElementObjects;
	
	p_total.genomicElementTypeObjects_count += p_usage.genomicElementTypeObjects_count;
	p_total.genomicElementTypeObjects += p_usage.genomicElementTypeObjects;
	
	p_total.individualObjects_count += p_usage.individualObjects_count;
	p_total.individualObjects += p_usage.individualObjects;
	p_total.individualUnusedPoolSpace += p_usage.individualUnusedPoolSpace;
	
	p_total.mutationObjects_count += p_usage.mutationObjects_count;
	p_total.mutationObjects += p_usage.mutationObjects;
	
	p_total.mutationRunObjects_count += p_usage.mutationRunObjects_count;
	p_total.mutationRunObjects += p_usage.mutationRunObjects;
	p_total.mutationRunExternalBuffers += p_usage.mutationRunExternalBuffers;
	p_total.mutationRunNonneutralCaches += p_usage.mutationRunNonneutralCaches;
	p_total.mutationRunUnusedPoolSpace += p_usage.mutationRunUnusedPoolSpace;
	p_total.mutationRunUnusedPoolBuffers += p_usage.mutationRunUnusedPoolBuffers;
	
	p_total.mutationTypeObjects_count += p_usage.mutationTypeObjects_count;
	p_total.mutationTypeObjects += p_usage.mutationTypeObjects;
	
	p_total.speciesObjects_count += p_usage.speciesObjects_count;
	p_total.speciesObjects += p_usage.speciesObjects;
	p_total.speciesTreeSeqTables += p_usage.speciesTreeSeqTables;
	
	p_total.subpopulationObjects_count += p_usage.subpopulationObjects_count;
	p_total.subpopulationObjects += p_usage.subpopulationObjects;
	p_total.subpopulationFitnessCaches += p_usage.subpopulationFitnessCaches;
	p_total.subpopulationParentTables += p_usage.subpopulationParentTables;
	p_total.subpopulationSpatialMaps += p_usage.subpopulationSpatialMaps;
	p_total.subpopulationSpatialMapsDisplay += p_usage.subpopulationSpatialMapsDisplay;
	
	p_total.substitutionObjects_count += p_usage.substitutionObjects_count;
	p_total.substitutionObjects += p_usage.substitutionObjects;
	
	p_total.totalMemoryUsage += p_usage.totalMemoryUsage;
}

void AccumulateMemoryUsageIntoTotal_Community(SLiMMemoryUsage_Community &p_usage, SLiMMemoryUsage_Community &p_total)
{
	// p_total += p_usage;
	
	p_total.communityObjects_count += p_usage.communityObjects_count;
	p_total.communityObjects += p_usage.communityObjects;
	
	p_total.mutationRefcountBuffer += p_usage.mutationRefcountBuffer;
	p_total.mutationUnusedPoolSpace += p_usage.mutationUnusedPoolSpace;
	
	p_total.interactionTypeObjects_count += p_usage.interactionTypeObjects_count;
	p_total.interactionTypeObjects += p_usage.interactionTypeObjects;
	p_total.interactionTypeKDTrees += p_usage.interactionTypeKDTrees;
	p_total.interactionTypePositionCaches += p_usage.interactionTypePositionCaches;
	
	p_total.interactionTypeSparseVectorPool += p_usage.interactionTypeSparseVectorPool;
	
	p_total.eidosASTNodePool += p_usage.eidosASTNodePool;
	p_total.eidosSymbolTablePool += p_usage.eidosSymbolTablePool;
	p_total.eidosValuePool += p_usage.eidosValuePool;
	p_total.fileBuffers += p_usage.fileBuffers;
	
	p_total.totalMemoryUsage += p_usage.totalMemoryUsage;
}



#pragma mark -
#pragma mark Shared SLiM types and enumerations
#pragma mark -

// Verbosity, from the command-line option -l[ong]; defaults to 1 if -l[ong] is not used
int64_t SLiM_verbosity_level = 1;

// stream output for cycle stages
std::string StringForSLiMCycleStage(SLiMCycleStage p_stage)
{
	switch (p_stage)
	{
		// some of these are not user-visible
		case SLiMCycleStage::kStagePreCycle: return "begin";
		case SLiMCycleStage::kWFStage0ExecuteFirstScripts: return "first";
		case SLiMCycleStage::kWFStage1ExecuteEarlyScripts: return "early";
		case SLiMCycleStage::kWFStage2GenerateOffspring: return "reproduction";
		case SLiMCycleStage::kWFStage3RemoveFixedMutations: return "tally";
		case SLiMCycleStage::kWFStage4SwapGenerations: return "swap";
		case SLiMCycleStage::kWFStage5ExecuteLateScripts: return "late";
		case SLiMCycleStage::kWFStage6CalculateFitness: return "fitness";
		case SLiMCycleStage::kWFStage7AdvanceTickCounter: return "end";
		case SLiMCycleStage::kNonWFStage0ExecuteFirstScripts: return "first";
		case SLiMCycleStage::kNonWFStage1GenerateOffspring: return "reproduction";
		case SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts: return "early";
		case SLiMCycleStage::kNonWFStage3CalculateFitness: return "fitness";
		case SLiMCycleStage::kNonWFStage4SurvivalSelection: return "survival";
		case SLiMCycleStage::kNonWFStage5RemoveFixedMutations: return "tally";
		case SLiMCycleStage::kNonWFStage6ExecuteLateScripts: return "late";
		case SLiMCycleStage::kNonWFStage7AdvanceTickCounter: return "end";
		case SLiMCycleStage::kStagePostCycle: return "console";
	}
	
	EIDOS_TERMINATION << "ERROR (StringForSLiMCycleStage): (internal) unrecognized cycle stage." << EidosTerminate();
}

// stream output for enumerations
std::string StringForGenomeType(GenomeType p_genome_type)
{
	switch (p_genome_type)
	{
		case GenomeType::kAutosome:		return gStr_A;
		case GenomeType::kXChromosome:	return gStr_X;		// SEX ONLY
		case GenomeType::kYChromosome:	return gStr_Y;		// SEX ONLY
	}
	EIDOS_TERMINATION << "ERROR (StringForGenomeType): (internal error) unexpected p_genome_type value." << EidosTerminate();
}

std::ostream& operator<<(std::ostream& p_out, GenomeType p_genome_type)
{
	p_out << StringForGenomeType(p_genome_type);
	return p_out;
}

std::string StringForIndividualSex(IndividualSex p_sex)
{
	switch (p_sex)
	{
		case IndividualSex::kUnspecified:		return "*";
		case IndividualSex::kHermaphrodite:		return "H";
		case IndividualSex::kFemale:			return "F";		// SEX ONLY
		case IndividualSex::kMale:				return "M";		// SEX ONLY
	}
	EIDOS_TERMINATION << "ERROR (StringForIndividualSex): (internal error) unexpected p_sex value." << EidosTerminate();
}

std::ostream& operator<<(std::ostream& p_out, IndividualSex p_sex)
{
	p_out << StringForIndividualSex(p_sex);
	return p_out;
}

const char gSLiM_Nucleotides[4] = {'A', 'C', 'G', 'T'};


#pragma mark -
#pragma mark NucleotideArray
#pragma mark -

NucleotideArray::NucleotideArray(std::size_t p_length, const int64_t *p_int_buffer) : length_(p_length)
{
	buffer_ = (uint64_t *)malloc(((length_ + 31) / 32) * sizeof(uint64_t));
	if (!buffer_)
		EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideArray): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
	
	// Eat 32 nucleotides at a time if we can
	std::size_t index = 0, buf_index = 0;
	
	for ( ; index < length_; index += 32)
	{
		uint64_t accumulator = 0;
		
		for (std::size_t i = 0; i < 32; )
		{
			uint64_t nuc = (uint64_t)p_int_buffer[index + i];
			
			if (nuc > 3)	// values < 0 will becomes > 3 after casting above
			{
				free(buffer_);
				buffer_ = nullptr;
				
				EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideArray): integer nucleotide value " << p_int_buffer[index + i] << " must be 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			}
			
			accumulator |= (nuc << (i * 2));
			
			if (index + ++i == length_)
				break;
		}
		
		buffer_[buf_index++] = accumulator;
	}
}

uint8_t *NucleotideArray::NucleotideCharToIntLookup(void)
{
	// set up a lookup table for speed
	static uint8_t *nuc_lookup = nullptr;
	
	if (!nuc_lookup)
	{
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("NucleotideArray::NucleotideCharToIntLookup(): usage of statics");
		
		nuc_lookup = (uint8_t *)malloc(256 * sizeof(uint8_t));
		if (!nuc_lookup)
			EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideCharToIntLookup): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
		
		for (int i = 0; i < 256; ++i)
			nuc_lookup[i] = 4;	// placeholder illegal value
		
		nuc_lookup[(int)('A')] = 0;
		nuc_lookup[(int)('C')] = 1;
		nuc_lookup[(int)('G')] = 2;
		nuc_lookup[(int)('T')] = 3;
	}
	
	return nuc_lookup;
}

NucleotideArray::NucleotideArray(std::size_t p_length, const char *p_char_buffer) : length_(p_length)
{
	uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
	
	buffer_ = (uint64_t *)malloc(((length_ + 31) / 32) * sizeof(uint64_t));
	if (!buffer_)
		EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideArray): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
	
	// Eat 32 nucleotides at a time if we can
	std::size_t index = 0, buf_index = 0;
	
	for ( ; index < length_; index += 32)
	{
		uint64_t accumulator = 0;
		
		for (std::size_t i = 0; i < 32; )
		{
			char nuc_char = p_char_buffer[index + i];
			uint64_t nuc = nuc_lookup[(int)(unsigned char)(nuc_char)];
			
			if (nuc > 3)
			{
				free(buffer_);
				buffer_ = nullptr;
				
				EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideArray): character nucleotide value '" << nuc_char << "' must be 'A', 'C', 'G', or 'T'." << EidosTerminate();
			}
			
			accumulator |= (nuc << (i * 2));
			
			if (index + ++i == length_)
				break;
		}
		
		buffer_[buf_index++] = accumulator;
	}
}

NucleotideArray::NucleotideArray(std::size_t p_length, const std::vector<std::string> &p_string_vector) : length_(p_length)
{
	buffer_ = (uint64_t *)malloc(((length_ + 31) / 32) * sizeof(uint64_t));
	if (!buffer_)
		EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideArray): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
	
	// Eat 32 nucleotides at a time if we can
	std::size_t index = 0, buf_index = 0;
	
	for ( ; index < length_; index += 32)
	{
		uint64_t accumulator = 0;
		
		for (std::size_t i = 0; i < 32; )
		{
			const std::string &nuc_string = p_string_vector[index + i];
			uint64_t nuc;
			
			if (nuc_string == gStr_A) nuc = 0;
			else if (nuc_string == gStr_C) nuc = 1;
			else if (nuc_string == gStr_G) nuc = 2;
			else if (nuc_string == gStr_T) nuc = 3;
			else
			{
				free(buffer_);
				buffer_ = nullptr;
				
				EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotideArray): string nucleotide character '" << nuc_string << "' must be 'A', 'C', 'G', or 'T'." << EidosTerminate();
			}
			
			accumulator |= (nuc << (i * 2));
			
			if (index + ++i == length_)
				break;
		}
		
		buffer_[buf_index++] = accumulator;
	}
}

void NucleotideArray::SetNucleotideAtIndex(std::size_t p_index, uint64_t p_nuc)
{
	if (p_nuc > 3)
		EIDOS_TERMINATION << "ERROR (NucleotideArray::SetNucleotideAtIndex): integer nucleotide values must be 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
	
	uint64_t &chunk = buffer_[p_index / 32];
	int shift = ((p_index % 32) * 2);
	uint64_t mask = ((uint64_t)0x03) << shift;
	uint64_t nucbits = (uint64_t)p_nuc << shift;
	
	chunk = (chunk & ~mask) | nucbits;
}

EidosValue_SP NucleotideArray::NucleotidesAsIntegerVector(int64_t start, int64_t end)
{
	int64_t length = end - start + 1;
	
	if (length == 1)
	{
		switch (NucleotideAtIndex(start))
		{
			case 0:		return gStaticEidosValue_Integer0;
			case 1:		return gStaticEidosValue_Integer1;
			case 2:		return gStaticEidosValue_Integer2;
			case 3:		return gStaticEidosValue_Integer3;
			default:
				EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotidesAsIntegerVector): nucleotide value out of range." << EidosTerminate();
		}
	}
	else
	{
		// return a vector of integers, 3 0 3 0
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length);
		
		for (int value_index = 0; value_index < length; ++value_index)
			int_result->set_int_no_check(NucleotideAtIndex(start + value_index), value_index);
		
		return EidosValue_SP(int_result);
	}
	
	return gStaticEidosValueNULL;
}

EidosValue_SP NucleotideArray::NucleotidesAsCodonVector(int64_t start, int64_t end, bool p_force_vector)
{
	int64_t length = end - start + 1;
	
	if ((length == 3) && !p_force_vector)
	{
		int nuc1 = NucleotideAtIndex(start);
		int nuc2 = NucleotideAtIndex(start + 1);
		int nuc3 = NucleotideAtIndex(start + 2);
		int codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(codon));
	}
	else
	{
		// return a vector of codons: nucleotide triplets compacted into a single integer value
		int64_t length_3 = length / 3;
		
		if (length % 3 != 0)
			EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotidesAsCodonVector): to obtain codons, the requested sequence length must be a multiple of 3." << EidosTerminate();
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length_3);
		
		for (int64_t value_index = 0; value_index < length_3; ++value_index)
		{
			int64_t codon_base = start + value_index * 3;
			
			int nuc1 = NucleotideAtIndex(codon_base);
			int nuc2 = NucleotideAtIndex(codon_base + 1);
			int nuc3 = NucleotideAtIndex(codon_base + 2);
			int codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
			
			int_result->set_int_no_check(codon, value_index);
		}
		
		return EidosValue_SP(int_result);
	}
}

EidosValue_SP NucleotideArray::NucleotidesAsStringVector(int64_t start, int64_t end)
{
	int64_t length = end - start + 1;
	
	if (length == 1)
	{
		switch (NucleotideAtIndex(start))
		{
			case 0:		return gStaticEidosValue_StringA;
			case 1:		return gStaticEidosValue_StringC;
			case 2:		return gStaticEidosValue_StringG;
			case 3:		return gStaticEidosValue_StringT;
			default:
				EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotidesAsStringVector): nucleotide value out of range." << EidosTerminate();
		}
	}
	else
	{
		// return a vector of one-character strings, "T" "A" "T" "A"
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)length);
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			switch (NucleotideAtIndex(start + value_index))
			{
				case 0:		string_result->PushString(gStr_A); break;
				case 1:		string_result->PushString(gStr_C); break;
				case 2:		string_result->PushString(gStr_G); break;
				case 3:		string_result->PushString(gStr_T); break;
				default:
					EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotidesAsStringVector): nucleotide value out of range." << EidosTerminate();
			}
		}
		
		return EidosValue_SP(string_result);
	}
	
	return gStaticEidosValueNULL;
}

EidosValue_SP NucleotideArray::NucleotidesAsStringSingleton(int64_t start, int64_t end)
{
	int64_t length = end - start + 1;
	
	if (length == 1)
	{
		switch (NucleotideAtIndex(start))
		{
			case 0:		return gStaticEidosValue_StringA;
			case 1:		return gStaticEidosValue_StringC;
			case 2:		return gStaticEidosValue_StringG;
			case 3:		return gStaticEidosValue_StringT;
			default:
				EIDOS_TERMINATION << "ERROR (NucleotideArray::NucleotidesAsStringSingleton): nucleotide value out of range." << EidosTerminate();
		}
	}
	else
	{
		// return a singleton string for the whole sequence, "TATA"; we munge the std::string inside the EidosValue to avoid memory copying, very naughty
		EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		std::string &nuc_string = string_result->StringValue_Mutable();
		
		nuc_string.resize(length);	// create space for all the nucleotides we will generate
		
		char *nuc_string_ptr = &nuc_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
		
		for (int value_index = 0; value_index < length; ++value_index)
			nuc_string_ptr[value_index] = gSLiM_Nucleotides[NucleotideAtIndex(start + value_index)];
		
		return EidosValue_SP(string_result);
	}
	
	return gStaticEidosValueNULL;
}

void NucleotideArray::WriteNucleotidesToBuffer(char *buffer) const
{
	for (std::size_t index = 0; index < length_; ++index)
		buffer[index] = gSLiM_Nucleotides[NucleotideAtIndex(index)];
}

void NucleotideArray::ReadNucleotidesFromBuffer(const char *buffer)
{
	for (std::size_t index = 0; index < length_; ++index)
	{
		char nuc_char = buffer[index];
		uint64_t nuc_int;
		
		if (nuc_char == 'A')		nuc_int = 0;
		else if (nuc_char == 'C')	nuc_int = 1;
		else if (nuc_char == 'G')	nuc_int = 2;
		else if (nuc_char == 'T')	nuc_int = 3;
		else EIDOS_TERMINATION << "ERROR (NucleotideArray::ReadNucleotidesFromBuffer): unexpected character '" << nuc_char << "' in nucleotide sequence." << EidosTerminate();
		
		SetNucleotideAtIndex(index, nuc_int);
	}
}

void NucleotideArray::WriteCompressedNucleotides(std::ostream &p_out) const
{
	// First write out the size of the sequence, in nucleotides, as a 64-bit int
	int64_t ancestral_sequence_size = (int64_t)size();
	
	p_out.write(reinterpret_cast<char *>(&ancestral_sequence_size), sizeof ancestral_sequence_size);
	
	// Then write out the compressed nucleotides themselves
	std::size_t size_bytes = ((ancestral_sequence_size + 31) / 32) * sizeof(uint64_t);
	
	p_out.write(reinterpret_cast<char *>(buffer_), size_bytes);
}

void NucleotideArray::ReadCompressedNucleotides(char **buffer, char *end)
{
	// First read the size of the sequence, in nucleotides, as a 64-bit int
	int64_t ancestral_sequence_size;
	
	if ((*buffer) + sizeof(ancestral_sequence_size) > end)
		EIDOS_TERMINATION << "ERROR (NucleotideArray::ReadCompressedNucleotides): out of buffer reading length." << EidosTerminate();
	
	ancestral_sequence_size = *(int64_t *)*buffer;
	(*buffer) += sizeof(ancestral_sequence_size);
	
	if ((std::size_t)ancestral_sequence_size != size())
		EIDOS_TERMINATION << "ERROR (NucleotideArray::ReadCompressedNucleotides): ancestral sequence length does not match the sequence length being read." << EidosTerminate();
	
	std::size_t size_bytes = ((ancestral_sequence_size + 31) / 32) * sizeof(uint64_t);
	
	if ((*buffer) + size_bytes > end)
		EIDOS_TERMINATION << "ERROR (NucleotideArray::ReadCompressedNucleotides): out of buffer reading nucleotides." << EidosTerminate();
	
	memcpy(buffer_, (*buffer), size_bytes);
	(*buffer) += size_bytes;
}

std::ostream& operator<<(std::ostream& p_out, const NucleotideArray &p_nuc_array)
{
	// Emit FASTA format with 70 bases per line
	std::size_t index = 0;
	std::string nuc_string;
	
	// Emit lines of length 70 first; presumably buffering in a string is faster than emitting one character at a time to the stream...
	nuc_string.resize(70);
	
	while (index + 70 <= p_nuc_array.length_)
	{
		for (int line_index = 0; line_index < 70; ++line_index)
			nuc_string[line_index] = gSLiM_Nucleotides[p_nuc_array.NucleotideAtIndex(index + line_index)];
		
		p_out << nuc_string << std::endl;
		index += 70;
	}
	
	// Then emit a final line with any remaining nucleotides
	if (index < p_nuc_array.length_)
	{
		for ( ; index < p_nuc_array.length_; ++index)
		{
			int nuc = p_nuc_array.NucleotideAtIndex(index);
			
			if (nuc == 0)			p_out << 'A';
			else if (nuc == 1)		p_out << 'C';
			else if (nuc == 2)		p_out << 'G';
			else /*if (nuc == 3)*/	p_out << 'T';
		}
		
		p_out << std::endl;
	}
	
	return p_out;
}

std::istream& operator>>(std::istream& p_in, NucleotideArray &p_nuc_array)
{
	// read in nucleotides, skipping over newline characters; we expect to read exactly the right number of nucleotides
	std::size_t index = 0;
	
	do
	{
		int nuc_char = p_in.get();
		
		if (nuc_char != EOF)
		{
			if ((nuc_char == '\r') || (nuc_char == '\n') || (nuc_char == ' '))
				continue;
			if (index >= p_nuc_array.length_)
				EIDOS_TERMINATION << "ERROR (NucleotideArray::operator>>): excess nucleotide sequence; the sequence length does not match the model." << EidosTerminate();
			
			uint64_t nuc_int;
			
			if (nuc_char == 'A')		nuc_int = 0;
			else if (nuc_char == 'C')	nuc_int = 1;
			else if (nuc_char == 'G')	nuc_int = 2;
			else if (nuc_char == 'T')	nuc_int = 3;
			else EIDOS_TERMINATION << "ERROR (NucleotideArray::operator>>): unexpected character '" << nuc_char << "' in nucleotide sequence." << EidosTerminate();
			
			p_nuc_array.SetNucleotideAtIndex(index, nuc_int);
			index++;
		}
		else
		{
			// we got an EOF; we should be exactly done
			if (index == p_nuc_array.length_)
				break;
			
			EIDOS_TERMINATION << "ERROR (NucleotideArray::operator>>): premature end of nucleotide sequence; the sequence length does not match the model." << EidosTerminate();
		}
	}
	while (true);
	
	return p_in;
}


#pragma mark -
#pragma mark Global strings and IDs
#pragma mark -

// initialize...() functions defined by Species
const std::string &gStr_initializeAncestralNucleotides = EidosRegisteredString("initializeAncestralNucleotides", gID_initializeAncestralNucleotides);
const std::string &gStr_initializeGenomicElement = EidosRegisteredString("initializeGenomicElement", gID_initializeGenomicElement);
const std::string &gStr_initializeGenomicElementType = EidosRegisteredString("initializeGenomicElementType", gID_initializeGenomicElementType);
const std::string &gStr_initializeMutationType = EidosRegisteredString("initializeMutationType", gID_initializeMutationType);
const std::string &gStr_initializeMutationTypeNuc = EidosRegisteredString("initializeMutationTypeNuc", gID_initializeMutationTypeNuc);
const std::string &gStr_initializeGeneConversion = EidosRegisteredString("initializeGeneConversion", gID_initializeGeneConversion);
const std::string &gStr_initializeMutationRate = EidosRegisteredString("initializeMutationRate", gID_initializeMutationRate);
const std::string &gStr_initializeHotspotMap = EidosRegisteredString("initializeHotspotMap", gID_initializeHotspotMap);
const std::string &gStr_initializeRecombinationRate = EidosRegisteredString("initializeRecombinationRate", gID_initializeRecombinationRate);
const std::string &gStr_initializeSex = EidosRegisteredString("initializeSex", gID_initializeSex);
const std::string &gStr_initializeSLiMOptions = EidosRegisteredString("initializeSLiMOptions", gID_initializeSLiMOptions);
const std::string &gStr_initializeSpecies = EidosRegisteredString("initializeSpecies", gID_initializeSpecies);
const std::string &gStr_initializeTreeSeq = EidosRegisteredString("initializeTreeSeq", gID_initializeTreeSeq);
const std::string &gStr_initializeSLiMModelType = EidosRegisteredString("initializeSLiMModelType", gID_initializeSLiMModelType);
const std::string &gStr_initializeInteractionType = EidosRegisteredString("initializeInteractionType", gID_initializeInteractionType);

// mostly property names
const std::string &gStr_genomicElements = EidosRegisteredString("genomicElements", gID_genomicElements);
const std::string &gStr_lastPosition = EidosRegisteredString("lastPosition", gID_lastPosition);
const std::string &gStr_hotspotEndPositions = EidosRegisteredString("hotspotEndPositions", gID_hotspotEndPositions);
const std::string &gStr_hotspotEndPositionsM = EidosRegisteredString("hotspotEndPositionsM", gID_hotspotEndPositionsM);
const std::string &gStr_hotspotEndPositionsF = EidosRegisteredString("hotspotEndPositionsF", gID_hotspotEndPositionsF);
const std::string &gStr_hotspotMultipliers = EidosRegisteredString("hotspotMultipliers", gID_hotspotMultipliers);
const std::string &gStr_hotspotMultipliersM = EidosRegisteredString("hotspotMultipliersM", gID_hotspotMultipliersM);
const std::string &gStr_hotspotMultipliersF = EidosRegisteredString("hotspotMultipliersF", gID_hotspotMultipliersF);
const std::string &gStr_mutationEndPositions = EidosRegisteredString("mutationEndPositions", gID_mutationEndPositions);
const std::string &gStr_mutationEndPositionsM = EidosRegisteredString("mutationEndPositionsM", gID_mutationEndPositionsM);
const std::string &gStr_mutationEndPositionsF = EidosRegisteredString("mutationEndPositionsF", gID_mutationEndPositionsF);
const std::string &gStr_mutationRates = EidosRegisteredString("mutationRates", gID_mutationRates);
const std::string &gStr_mutationRatesM = EidosRegisteredString("mutationRatesM", gID_mutationRatesM);
const std::string &gStr_mutationRatesF = EidosRegisteredString("mutationRatesF", gID_mutationRatesF);
const std::string &gStr_overallMutationRate = EidosRegisteredString("overallMutationRate", gID_overallMutationRate);
const std::string &gStr_overallMutationRateM = EidosRegisteredString("overallMutationRateM", gID_overallMutationRateM);
const std::string &gStr_overallMutationRateF = EidosRegisteredString("overallMutationRateF", gID_overallMutationRateF);
const std::string &gStr_overallRecombinationRate = EidosRegisteredString("overallRecombinationRate", gID_overallRecombinationRate);
const std::string &gStr_overallRecombinationRateM = EidosRegisteredString("overallRecombinationRateM", gID_overallRecombinationRateM);
const std::string &gStr_overallRecombinationRateF = EidosRegisteredString("overallRecombinationRateF", gID_overallRecombinationRateF);
const std::string &gStr_recombinationEndPositions = EidosRegisteredString("recombinationEndPositions", gID_recombinationEndPositions);
const std::string &gStr_recombinationEndPositionsM = EidosRegisteredString("recombinationEndPositionsM", gID_recombinationEndPositionsM);
const std::string &gStr_recombinationEndPositionsF = EidosRegisteredString("recombinationEndPositionsF", gID_recombinationEndPositionsF);
const std::string &gStr_recombinationRates = EidosRegisteredString("recombinationRates", gID_recombinationRates);
const std::string &gStr_recombinationRatesM = EidosRegisteredString("recombinationRatesM", gID_recombinationRatesM);
const std::string &gStr_recombinationRatesF = EidosRegisteredString("recombinationRatesF", gID_recombinationRatesF);
const std::string &gStr_geneConversionEnabled = EidosRegisteredString("geneConversionEnabled", gID_geneConversionEnabled);
const std::string &gStr_geneConversionGCBias = EidosRegisteredString("geneConversionGCBias", gID_geneConversionGCBias);
const std::string &gStr_geneConversionNonCrossoverFraction = EidosRegisteredString("geneConversionNonCrossoverFraction", gID_geneConversionNonCrossoverFraction);
const std::string &gStr_geneConversionMeanLength = EidosRegisteredString("geneConversionMeanLength", gID_geneConversionMeanLength);
const std::string &gStr_geneConversionSimpleConversionFraction = EidosRegisteredString("geneConversionSimpleConversionFraction", gID_geneConversionSimpleConversionFraction);
const std::string &gStr_genomeType = EidosRegisteredString("genomeType", gID_genomeType);
const std::string &gStr_isNullGenome = EidosRegisteredString("isNullGenome", gID_isNullGenome);
const std::string &gStr_mutations = EidosRegisteredString("mutations", gID_mutations);
const std::string &gStr_uniqueMutations = EidosRegisteredString("uniqueMutations", gID_uniqueMutations);
const std::string &gStr_genomicElementType = EidosRegisteredString("genomicElementType", gID_genomicElementType);
const std::string &gStr_startPosition = EidosRegisteredString("startPosition", gID_startPosition);
const std::string &gStr_endPosition = EidosRegisteredString("endPosition", gID_endPosition);
const std::string &gStr_id = EidosRegisteredString("id", gID_id);
const std::string &gStr_mutationTypes = EidosRegisteredString("mutationTypes", gID_mutationTypes);
const std::string &gStr_mutationFractions = EidosRegisteredString("mutationFractions", gID_mutationFractions);
const std::string &gStr_mutationMatrix = EidosRegisteredString("mutationMatrix", gID_mutationMatrix);
const std::string &gStr_isFixed = EidosRegisteredString("isFixed", gID_isFixed);
const std::string &gStr_isSegregating = EidosRegisteredString("isSegregating", gID_isSegregating);
const std::string &gStr_mutationType = EidosRegisteredString("mutationType", gID_mutationType);
const std::string &gStr_nucleotide = EidosRegisteredString("nucleotide", gID_nucleotide);
const std::string &gStr_nucleotideValue = EidosRegisteredString("nucleotideValue", gID_nucleotideValue);
const std::string &gStr_originTick = EidosRegisteredString("originTick", gID_originTick);
const std::string &gStr_position = EidosRegisteredString("position", gID_position);
const std::string &gStr_selectionCoeff = EidosRegisteredString("selectionCoeff", gID_selectionCoeff);
const std::string &gStr_subpopID = EidosRegisteredString("subpopID", gID_subpopID);
const std::string &gStr_convertToSubstitution = EidosRegisteredString("convertToSubstitution", gID_convertToSubstitution);
const std::string &gStr_distributionType = EidosRegisteredString("distributionType", gID_distributionType);
const std::string &gStr_distributionParams = EidosRegisteredString("distributionParams", gID_distributionParams);
const std::string &gStr_dominanceCoeff = EidosRegisteredString("dominanceCoeff", gID_dominanceCoeff);
const std::string &gStr_haploidDominanceCoeff = EidosRegisteredString("haploidDominanceCoeff", gID_haploidDominanceCoeff);
const std::string &gStr_mutationStackGroup = EidosRegisteredString("mutationStackGroup", gID_mutationStackGroup);
const std::string &gStr_mutationStackPolicy = EidosRegisteredString("mutationStackPolicy", gID_mutationStackPolicy);
//const std::string &gStr_start = EidosRegisteredString("start", gID_start);
//const std::string &gStr_end = EidosRegisteredString("end", gID_end);
//const std::string &gStr_type = EidosRegisteredString("type", gID_type);
//const std::string &gStr_source = EidosRegisteredString("source", gID_source);
const std::string &gStr_active = EidosRegisteredString("active", gID_active);
const std::string &gStr_allGenomicElementTypes = EidosRegisteredString("allGenomicElementTypes", gID_allGenomicElementTypes);
const std::string &gStr_allInteractionTypes = EidosRegisteredString("allInteractionTypes", gID_allInteractionTypes);
const std::string &gStr_allMutationTypes = EidosRegisteredString("allMutationTypes", gID_allMutationTypes);
const std::string &gStr_allScriptBlocks = EidosRegisteredString("allScriptBlocks", gID_allScriptBlocks);
const std::string &gStr_allSpecies = EidosRegisteredString("allSpecies", gID_allSpecies);
const std::string &gStr_allSubpopulations = EidosRegisteredString("allSubpopulations", gID_allSubpopulations);
const std::string &gStr_chromosome = EidosRegisteredString("chromosome", gID_chromosome);
const std::string &gStr_chromosomeType = EidosRegisteredString("chromosomeType", gID_chromosomeType);
const std::string &gStr_genomicElementTypes = EidosRegisteredString("genomicElementTypes", gID_genomicElementTypes);
const std::string &gStr_lifetimeReproductiveOutput = EidosRegisteredString("lifetimeReproductiveOutput", gID_lifetimeReproductiveOutput);
const std::string &gStr_lifetimeReproductiveOutputM = EidosRegisteredString("lifetimeReproductiveOutputM", gID_lifetimeReproductiveOutputM);
const std::string &gStr_lifetimeReproductiveOutputF = EidosRegisteredString("lifetimeReproductiveOutputF", gID_lifetimeReproductiveOutputF);
const std::string &gStr_modelType = EidosRegisteredString("modelType", gID_modelType);
const std::string &gStr_nucleotideBased = EidosRegisteredString("nucleotideBased", gID_nucleotideBased);
const std::string &gStr_scriptBlocks = EidosRegisteredString("scriptBlocks", gID_scriptBlocks);
const std::string &gStr_sexEnabled = EidosRegisteredString("sexEnabled", gID_sexEnabled);
const std::string &gStr_subpopulations = EidosRegisteredString("subpopulations", gID_subpopulations);
const std::string &gStr_substitutions = EidosRegisteredString("substitutions", gID_substitutions);
const std::string &gStr_tick = EidosRegisteredString("tick", gID_tick);
const std::string &gStr_cycle = EidosRegisteredString("cycle", gID_cycle);
const std::string &gStr_cycleStage = EidosRegisteredString("cycleStage", gID_cycleStage);
const std::string &gStr_colorSubstitution = EidosRegisteredString("colorSubstitution", gID_colorSubstitution);
const std::string &gStr_verbosity = EidosRegisteredString("verbosity", gID_verbosity);
const std::string &gStr_tag = EidosRegisteredString("tag", gID_tag);
const std::string &gStr_tagF = EidosRegisteredString("tagF", gID_tagF);
const std::string &gStr_tagL0 = EidosRegisteredString("tagL0", gID_tagL0);
const std::string &gStr_tagL1 = EidosRegisteredString("tagL1", gID_tagL1);
const std::string &gStr_tagL2 = EidosRegisteredString("tagL2", gID_tagL2);
const std::string &gStr_tagL3 = EidosRegisteredString("tagL3", gID_tagL3);
const std::string &gStr_tagL4 = EidosRegisteredString("tagL4", gID_tagL4);
const std::string &gStr_migrant = EidosRegisteredString("migrant", gID_migrant);
const std::string &gStr_fitnessScaling = EidosRegisteredString("fitnessScaling", gID_fitnessScaling);
const std::string &gStr_firstMaleIndex = EidosRegisteredString("firstMaleIndex", gID_firstMaleIndex);
const std::string &gStr_genomes = EidosRegisteredString("genomes", gID_genomes);
const std::string &gStr_genomesNonNull = EidosRegisteredString("genomesNonNull", gID_genomesNonNull);
const std::string &gStr_sex = EidosRegisteredString("sex", gID_sex);
const std::string &gStr_individuals = EidosRegisteredString("individuals", gID_individuals);
const std::string &gStr_subpopulation = EidosRegisteredString("subpopulation", gID_subpopulation);
const std::string &gStr_index = EidosRegisteredString("index", gID_index);
const std::string &gStr_immigrantSubpopIDs = EidosRegisteredString("immigrantSubpopIDs", gID_immigrantSubpopIDs);
const std::string &gStr_immigrantSubpopFractions = EidosRegisteredString("immigrantSubpopFractions", gID_immigrantSubpopFractions);
const std::string &gStr_avatar = EidosRegisteredString("avatar", gID_avatar);
const std::string &gStr_name = EidosRegisteredString("name", gID_name);
const std::string &gStr_description = EidosRegisteredString("description", gID_description);
const std::string &gStr_selfingRate = EidosRegisteredString("selfingRate", gID_selfingRate);
const std::string &gStr_cloningRate = EidosRegisteredString("cloningRate", gID_cloningRate);
const std::string &gStr_sexRatio = EidosRegisteredString("sexRatio", gID_sexRatio);
const std::string &gStr_gridDimensions = EidosRegisteredString("gridDimensions", gID_gridDimensions);
const std::string &gStr_interpolate = EidosRegisteredString("interpolate", gID_interpolate);
const std::string &gStr_spatialBounds = EidosRegisteredString("spatialBounds", gID_spatialBounds);
const std::string &gStr_spatialMaps = EidosRegisteredString("spatialMaps", gID_spatialMaps);
const std::string &gStr_individualCount = EidosRegisteredString("individualCount", gID_individualCount);
const std::string &gStr_fixationTick = EidosRegisteredString("fixationTick", gID_fixationTick);
const std::string &gStr_age = EidosRegisteredString("age", gID_age);
const std::string &gStr_meanParentAge = EidosRegisteredString("meanParentAge", gID_meanParentAge);
const std::string &gStr_pedigreeID = EidosRegisteredString("pedigreeID", gID_pedigreeID);
const std::string &gStr_pedigreeParentIDs = EidosRegisteredString("pedigreeParentIDs", gID_pedigreeParentIDs);
const std::string &gStr_pedigreeGrandparentIDs = EidosRegisteredString("pedigreeGrandparentIDs", gID_pedigreeGrandparentIDs);
const std::string &gStr_reproductiveOutput = EidosRegisteredString("reproductiveOutput", gID_reproductiveOutput);
const std::string &gStr_genomePedigreeID = EidosRegisteredString("genomePedigreeID", gID_genomePedigreeID);
const std::string &gStr_reciprocal = EidosRegisteredString("reciprocal", gID_reciprocal);
const std::string &gStr_sexSegregation = EidosRegisteredString("sexSegregation", gID_sexSegregation);
const std::string &gStr_dimensionality = EidosRegisteredString("dimensionality", gID_dimensionality);
const std::string &gStr_periodicity = EidosRegisteredString("periodicity", gID_periodicity);
const std::string &gStr_spatiality = EidosRegisteredString("spatiality", gID_spatiality);
const std::string &gStr_spatialPosition = EidosRegisteredString("spatialPosition", gID_spatialPosition);
const std::string &gStr_maxDistance = EidosRegisteredString("maxDistance", gID_maxDistance);

// mostly method names
const std::string &gStr_ancestralNucleotides = EidosRegisteredString("ancestralNucleotides", gID_ancestralNucleotides);
const std::string &gStr_nucleotides = EidosRegisteredString("nucleotides", gID_nucleotides);
const std::string &gStr_setAncestralNucleotides = EidosRegisteredString("setAncestralNucleotides", gID_setAncestralNucleotides);
const std::string &gStr_setGeneConversion = EidosRegisteredString("setGeneConversion", gID_setGeneConversion);
const std::string &gStr_setHotspotMap = EidosRegisteredString("setHotspotMap", gID_setHotspotMap);
const std::string &gStr_setMutationRate = EidosRegisteredString("setMutationRate", gID_setMutationRate);
const std::string &gStr_setRecombinationRate = EidosRegisteredString("setRecombinationRate", gID_setRecombinationRate);
const std::string &gStr_drawBreakpoints = EidosRegisteredString("drawBreakpoints", gID_drawBreakpoints);
const std::string &gStr_addMutations = EidosRegisteredString("addMutations", gID_addMutations);
const std::string &gStr_addNewDrawnMutation = EidosRegisteredString("addNewDrawnMutation", gID_addNewDrawnMutation);
const std::string &gStr_addNewMutation = EidosRegisteredString("addNewMutation", gID_addNewMutation);
const std::string &gStr_containsMutations = EidosRegisteredString("containsMutations", gID_containsMutations);
const std::string &gStr_countOfMutationsOfType = EidosRegisteredString("countOfMutationsOfType", gID_countOfMutationsOfType);
const std::string &gStr_positionsOfMutationsOfType = EidosRegisteredString("positionsOfMutationsOfType", gID_positionsOfMutationsOfType);
const std::string &gStr_containsMarkerMutation = EidosRegisteredString("containsMarkerMutation", gID_containsMarkerMutation);
const std::string &gStr_relatedness = EidosRegisteredString("relatedness", gID_relatedness);
const std::string &gStr_sharedParentCount = EidosRegisteredString("sharedParentCount", gID_sharedParentCount);
const std::string &gStr_mutationsOfType = EidosRegisteredString("mutationsOfType", gID_mutationsOfType);
const std::string &gStr_setSpatialPosition = EidosRegisteredString("setSpatialPosition", gID_setSpatialPosition);
const std::string &gStr_sumOfMutationsOfType = EidosRegisteredString("sumOfMutationsOfType", gID_sumOfMutationsOfType);
const std::string &gStr_uniqueMutationsOfType = EidosRegisteredString("uniqueMutationsOfType", gID_uniqueMutationsOfType);
const std::string &gStr_readFromMS = EidosRegisteredString("readFromMS", gID_readFromMS);
const std::string &gStr_readFromVCF = EidosRegisteredString("readFromVCF", gID_readFromVCF);
const std::string &gStr_removeMutations = EidosRegisteredString("removeMutations", gID_removeMutations);
const std::string &gStr_setGenomicElementType = EidosRegisteredString("setGenomicElementType", gID_setGenomicElementType);
const std::string &gStr_setMutationFractions = EidosRegisteredString("setMutationFractions", gID_setMutationFractions);
const std::string &gStr_setMutationMatrix = EidosRegisteredString("setMutationMatrix", gID_setMutationMatrix);
const std::string &gStr_setSelectionCoeff = EidosRegisteredString("setSelectionCoeff", gID_setSelectionCoeff);
const std::string &gStr_setMutationType = EidosRegisteredString("setMutationType", gID_setMutationType);
const std::string &gStr_drawSelectionCoefficient = EidosRegisteredString("drawSelectionCoefficient", gID_drawSelectionCoefficient);
const std::string &gStr_setDistribution = EidosRegisteredString("setDistribution", gID_setDistribution);
const std::string &gStr_addSubpop = EidosRegisteredString("addSubpop", gID_addSubpop);
const std::string &gStr_addSubpopSplit = EidosRegisteredString("addSubpopSplit", gID_addSubpopSplit);
const std::string &gStr_deregisterScriptBlock = EidosRegisteredString("deregisterScriptBlock", gID_deregisterScriptBlock);
const std::string &gStr_genomicElementTypesWithIDs = EidosRegisteredString("genomicElementTypesWithIDs", gID_genomicElementTypesWithIDs);
const std::string &gStr_interactionTypesWithIDs = EidosRegisteredString("interactionTypesWithIDs", gID_interactionTypesWithIDs);
const std::string &gStr_mutationTypesWithIDs = EidosRegisteredString("mutationTypesWithIDs", gID_mutationTypesWithIDs);
const std::string &gStr_scriptBlocksWithIDs = EidosRegisteredString("scriptBlocksWithIDs", gID_scriptBlocksWithIDs);
const std::string &gStr_speciesWithIDs = EidosRegisteredString("speciesWithIDs", gID_speciesWithIDs);
const std::string &gStr_subpopulationsWithIDs = EidosRegisteredString("subpopulationsWithIDs", gID_subpopulationsWithIDs);
const std::string &gStr_individualsWithPedigreeIDs = EidosRegisteredString("individualsWithPedigreeIDs", gID_individualsWithPedigreeIDs);
const std::string &gStr_killIndividuals = EidosRegisteredString("killIndividuals", gID_killIndividuals);
const std::string &gStr_mutationCounts = EidosRegisteredString("mutationCounts", gID_mutationCounts);
const std::string &gStr_mutationCountsInGenomes = EidosRegisteredString("mutationCountsInGenomes", gID_mutationCountsInGenomes);
const std::string &gStr_mutationFrequencies = EidosRegisteredString("mutationFrequencies", gID_mutationFrequencies);
const std::string &gStr_mutationFrequenciesInGenomes = EidosRegisteredString("mutationFrequenciesInGenomes", gID_mutationFrequenciesInGenomes);
//const std::string &gStr_mutationsOfType = EidosRegisteredString("mutationsOfType", gID_mutationsOfType);
//const std::string &gStr_countOfMutationsOfType = EidosRegisteredString("countOfMutationsOfType", gID_countOfMutationsOfType);
const std::string &gStr_outputFixedMutations = EidosRegisteredString("outputFixedMutations", gID_outputFixedMutations);
const std::string &gStr_outputFull = EidosRegisteredString("outputFull", gID_outputFull);
const std::string &gStr_outputMutations = EidosRegisteredString("outputMutations", gID_outputMutations);
const std::string &gStr_outputUsage = EidosRegisteredString("outputUsage", gID_outputUsage);
const std::string &gStr_readFromPopulationFile = EidosRegisteredString("readFromPopulationFile", gID_readFromPopulationFile);
const std::string &gStr_recalculateFitness = EidosRegisteredString("recalculateFitness", gID_recalculateFitness);
const std::string &gStr_registerFirstEvent = EidosRegisteredString("registerFirstEvent", gID_registerFirstEvent);
const std::string &gStr_registerEarlyEvent = EidosRegisteredString("registerEarlyEvent", gID_registerEarlyEvent);
const std::string &gStr_registerLateEvent = EidosRegisteredString("registerLateEvent", gID_registerLateEvent);
const std::string &gStr_registerFitnessEffectCallback = EidosRegisteredString("registerFitnessEffectCallback", gID_registerFitnessEffectCallback);
const std::string &gStr_registerInteractionCallback = EidosRegisteredString("registerInteractionCallback", gID_registerInteractionCallback);
const std::string &gStr_registerMateChoiceCallback = EidosRegisteredString("registerMateChoiceCallback", gID_registerMateChoiceCallback);
const std::string &gStr_registerModifyChildCallback = EidosRegisteredString("registerModifyChildCallback", gID_registerModifyChildCallback);
const std::string &gStr_registerRecombinationCallback = EidosRegisteredString("registerRecombinationCallback", gID_registerRecombinationCallback);
const std::string &gStr_registerMutationCallback = EidosRegisteredString("registerMutationCallback", gID_registerMutationCallback);
const std::string &gStr_registerMutationEffectCallback = EidosRegisteredString("registerMutationEffectCallback", gID_registerMutationEffectCallback);
const std::string &gStr_registerSurvivalCallback = EidosRegisteredString("registerSurvivalCallback", gID_registerSurvivalCallback);
const std::string &gStr_registerReproductionCallback = EidosRegisteredString("registerReproductionCallback", gID_registerReproductionCallback);
const std::string &gStr_rescheduleScriptBlock = EidosRegisteredString("rescheduleScriptBlock", gID_rescheduleScriptBlock);
const std::string &gStr_simulationFinished = EidosRegisteredString("simulationFinished", gID_simulationFinished);
const std::string &gStr_skipTick = EidosRegisteredString("skipTick", gID_skipTick);
const std::string &gStr_subsetMutations = EidosRegisteredString("subsetMutations", gID_subsetMutations);
const std::string &gStr_treeSeqCoalesced = EidosRegisteredString("treeSeqCoalesced", gID_treeSeqCoalesced);
const std::string &gStr_treeSeqSimplify = EidosRegisteredString("treeSeqSimplify", gID_treeSeqSimplify);
const std::string &gStr_treeSeqRememberIndividuals = EidosRegisteredString("treeSeqRememberIndividuals", gID_treeSeqRememberIndividuals);
const std::string &gStr_treeSeqOutput = EidosRegisteredString("treeSeqOutput", gID_treeSeqOutput);
const std::string &gStr_setMigrationRates = EidosRegisteredString("setMigrationRates", gID_setMigrationRates);
const std::string &gStr_pointDeviated = EidosRegisteredString("pointDeviated", gID_pointDeviated);
const std::string &gStr_pointInBounds = EidosRegisteredString("pointInBounds", gID_pointInBounds);
const std::string &gStr_pointReflected = EidosRegisteredString("pointReflected", gID_pointReflected);
const std::string &gStr_pointStopped = EidosRegisteredString("pointStopped", gID_pointStopped);
const std::string &gStr_pointPeriodic = EidosRegisteredString("pointPeriodic", gID_pointPeriodic);
const std::string &gStr_pointUniform = EidosRegisteredString("pointUniform", gID_pointUniform);
const std::string &gStr_setCloningRate = EidosRegisteredString("setCloningRate", gID_setCloningRate);
const std::string &gStr_setSelfingRate = EidosRegisteredString("setSelfingRate", gID_setSelfingRate);
const std::string &gStr_setSexRatio = EidosRegisteredString("setSexRatio", gID_setSexRatio);
const std::string &gStr_setSpatialBounds = EidosRegisteredString("setSpatialBounds", gID_setSpatialBounds);
const std::string &gStr_setSubpopulationSize = EidosRegisteredString("setSubpopulationSize", gID_setSubpopulationSize);
const std::string &gStr_addCloned = EidosRegisteredString("addCloned", gID_addCloned);
const std::string &gStr_addCrossed = EidosRegisteredString("addCrossed", gID_addCrossed);
const std::string &gStr_addEmpty = EidosRegisteredString("addEmpty", gID_addEmpty);
const std::string &gStr_addRecombinant = EidosRegisteredString("addRecombinant", gID_addRecombinant);
const std::string &gStr_addSelfed = EidosRegisteredString("addSelfed", gID_addSelfed);
const std::string &gStr_takeMigrants = EidosRegisteredString("takeMigrants", gID_takeMigrants);
const std::string &gStr_removeSubpopulation = EidosRegisteredString("removeSubpopulation", gID_removeSubpopulation);
const std::string &gStr_cachedFitness = EidosRegisteredString("cachedFitness", gID_cachedFitness);
const std::string &gStr_sampleIndividuals = EidosRegisteredString("sampleIndividuals", gID_sampleIndividuals);
const std::string &gStr_subsetIndividuals = EidosRegisteredString("subsetIndividuals", gID_subsetIndividuals);
const std::string &gStr_defineSpatialMap = EidosRegisteredString("defineSpatialMap", gID_defineSpatialMap);
const std::string &gStr_addSpatialMap = EidosRegisteredString("addSpatialMap", gID_addSpatialMap);
const std::string &gStr_removeSpatialMap = EidosRegisteredString("removeSpatialMap", gID_removeSpatialMap);
const std::string &gStr_spatialMapColor = EidosRegisteredString("spatialMapColor", gID_spatialMapColor);
const std::string &gStr_spatialMapImage = EidosRegisteredString("spatialMapImage", gID_spatialMapImage);
const std::string &gStr_spatialMapValue = EidosRegisteredString("spatialMapValue", gID_spatialMapValue);
const std::string &gStr_add = EidosRegisteredString("add", gID_add);
const std::string &gStr_blend = EidosRegisteredString("blend", gID_blend);
const std::string &gStr_multiply = EidosRegisteredString("multiply", gID_multiply);
const std::string &gStr_subtract = EidosRegisteredString("subtract", gID_subtract);
const std::string &gStr_divide = EidosRegisteredString("divide", gID_divide);
const std::string &gStr_power = EidosRegisteredString("power", gID_power);
const std::string &gStr_exp = EidosRegisteredString("exp", gID_exp);
const std::string &gStr_changeColors = EidosRegisteredString("changeColors", gID_changeColors);
const std::string &gStr_changeValues = EidosRegisteredString("changeValues", gID_changeValues);
const std::string &gStr_gridValues = EidosRegisteredString("gridValues", gID_gridValues);
const std::string &gStr_mapColor = EidosRegisteredString("mapColor", gID_mapColor);
const std::string &gStr_mapImage = EidosRegisteredString("mapImage", gID_mapImage);
const std::string &gStr_mapValue = EidosRegisteredString("mapValue", gID_mapValue);
const std::string &gStr_rescale = EidosRegisteredString("rescale", gID_rescale);
const std::string &gStr_sampleImprovedNearbyPoint = EidosRegisteredString("sampleImprovedNearbyPoint", gID_sampleImprovedNearbyPoint);
const std::string &gStr_sampleNearbyPoint = EidosRegisteredString("sampleNearbyPoint", gID_sampleNearbyPoint);
const std::string &gStr_smooth = EidosRegisteredString("smooth", gID_smooth);
const std::string &gStr_outputMSSample = EidosRegisteredString("outputMSSample", gID_outputMSSample);
const std::string &gStr_outputVCFSample = EidosRegisteredString("outputVCFSample", gID_outputVCFSample);
const std::string &gStr_outputSample = EidosRegisteredString("outputSample", gID_outputSample);
const std::string &gStr_outputMS = EidosRegisteredString("outputMS", gID_outputMS);
const std::string &gStr_outputVCF = EidosRegisteredString("outputVCF", gID_outputVCF);
const std::string &gStr_output = EidosRegisteredString("output", gID_output);
const std::string &gStr_evaluate = EidosRegisteredString("evaluate", gID_evaluate);
const std::string &gStr_distance = EidosRegisteredString("distance", gID_distance);
const std::string &gStr_localPopulationDensity = EidosRegisteredString("localPopulationDensity", gID_localPopulationDensity);
const std::string &gStr_interactionDistance = EidosRegisteredString("interactionDistance", gID_interactionDistance);
const std::string &gStr_clippedIntegral = EidosRegisteredString("clippedIntegral", gID_clippedIntegral);
const std::string &gStr_distanceFromPoint = EidosRegisteredString("distanceFromPoint", gID_distanceFromPoint);
const std::string &gStr_nearestNeighbors = EidosRegisteredString("nearestNeighbors", gID_nearestNeighbors);
const std::string &gStr_neighborCount = EidosRegisteredString("neighborCount", gID_neighborCount);
const std::string &gStr_neighborCountOfPoint = EidosRegisteredString("neighborCountOfPoint", gID_neighborCountOfPoint);
const std::string &gStr_nearestInteractingNeighbors = EidosRegisteredString("nearestInteractingNeighbors", gID_nearestInteractingNeighbors);
const std::string &gStr_interactingNeighborCount = EidosRegisteredString("interactingNeighborCount", gID_interactingNeighborCount);
const std::string &gStr_nearestNeighborsOfPoint = EidosRegisteredString("nearestNeighborsOfPoint", gID_nearestNeighborsOfPoint);
const std::string &gStr_setConstraints = EidosRegisteredString("setConstraints", gID_setConstraints);
const std::string &gStr_setInteractionFunction = EidosRegisteredString("setInteractionFunction", gID_setInteractionFunction);
const std::string &gStr_strength = EidosRegisteredString("strength", gID_strength);
const std::string &gStr_testConstraints = EidosRegisteredString("testConstraints", gID_testConstraints);
const std::string &gStr_totalOfNeighborStrengths = EidosRegisteredString("totalOfNeighborStrengths", gID_totalOfNeighborStrengths);
const std::string &gStr_unevaluate = EidosRegisteredString("unevaluate", gID_unevaluate);
const std::string &gStr_drawByStrength = EidosRegisteredString("drawByStrength", gID_drawByStrength);

// mostly SLiM variable names used in callbacks and such
const std::string &gStr_community = EidosRegisteredString("community", gID_community);
const std::string &gStr_sim = EidosRegisteredString("sim", gID_sim);
const std::string &gStr_self = EidosRegisteredString("self", gID_self);
const std::string &gStr_individual = EidosRegisteredString("individual", gID_individual);
const std::string &gStr_element = EidosRegisteredString("element", gID_element);
const std::string &gStr_genome = EidosRegisteredString("genome", gID_genome);
const std::string &gStr_genome1 = EidosRegisteredString("genome1", gID_genome1);
const std::string &gStr_genome2 = EidosRegisteredString("genome2", gID_genome2);
const std::string &gStr_subpop = EidosRegisteredString("subpop", gID_subpop);
const std::string &gStr_sourceSubpop = EidosRegisteredString("sourceSubpop", gID_sourceSubpop);
//const std::string &gStr_weights = EidosRegisteredString("weights", gID_weights);		now gEidosStr_weights
const std::string &gStr_child = EidosRegisteredString("child", gID_child);
const std::string &gStr_parent = EidosRegisteredString("parent", gID_parent);
const std::string &gStr_parent1 = EidosRegisteredString("parent1", gID_parent1);
const std::string &gStr_isCloning = EidosRegisteredString("isCloning", gID_isCloning);
const std::string &gStr_isSelfing = EidosRegisteredString("isSelfing", gID_isSelfing);
const std::string &gStr_parent2 = EidosRegisteredString("parent2", gID_parent2);
const std::string &gStr_mut = EidosRegisteredString("mut", gID_mut);
const std::string &gStr_effect = EidosRegisteredString("effect", gID_effect);
const std::string &gStr_homozygous = EidosRegisteredString("homozygous", gID_homozygous);
const std::string &gStr_breakpoints = EidosRegisteredString("breakpoints", gID_breakpoints);
const std::string &gStr_receiver = EidosRegisteredString("receiver", gID_receiver);
const std::string &gStr_exerter = EidosRegisteredString("exerter", gID_exerter);
const std::string &gStr_originalNuc = EidosRegisteredString("originalNuc", gID_originalNuc);
const std::string &gStr_fitness = EidosRegisteredString("fitness", gID_fitness);
const std::string &gStr_surviving = EidosRegisteredString("surviving", gID_surviving);
const std::string &gStr_draw = EidosRegisteredString("draw", gID_draw);

// SLiMgui instance name and methods
const std::string &gStr_slimgui = EidosRegisteredString("slimgui", gID_slimgui);
const std::string &gStr_pid = EidosRegisteredString("pid", gID_pid);
const std::string &gStr_openDocument = EidosRegisteredString("openDocument", gID_openDocument);
const std::string &gStr_pauseExecution = EidosRegisteredString("pauseExecution", gID_pauseExecution);
const std::string &gStr_configureDisplay = EidosRegisteredString("configureDisplay", gID_configureDisplay);

// mostly SLiM element types
const std::string &gStr_Chromosome = EidosRegisteredString("Chromosome", gID_Chromosome);
//const std::string &gStr_Genome = EidosRegisteredString("Genome", gID_Genome);				// in Eidos; see EidosValue_Object::EidosValue_Object()
const std::string &gStr_GenomicElement = EidosRegisteredString("GenomicElement", gID_GenomicElement);
const std::string &gStr_GenomicElementType = EidosRegisteredString("GenomicElementType", gID_GenomicElementType);
//const std::string &gStr_Mutation = EidosRegisteredString("Mutation", gID_Mutation);			// in Eidos; see EidosValue_Object::EidosValue_Object()
const std::string &gStr_MutationType = EidosRegisteredString("MutationType", gID_MutationType);
const std::string &gStr_SLiMEidosBlock = EidosRegisteredString("SLiMEidosBlock", gID_SLiMEidosBlock);
const std::string &gStr_Community = EidosRegisteredString("Community", gID_Community);
const std::string &gStr_SpatialMap = EidosRegisteredString("SpatialMap", gID_SpatialMap);
const std::string &gStr_Species = EidosRegisteredString("Species", gID_Species);
const std::string &gStr_Subpopulation = EidosRegisteredString("Subpopulation", gID_Subpopulation);
//const std::string &gStr_Individual = EidosRegisteredString("Individual", gID_Individual);		// in Eidos; see EidosValue_Object::EidosValue_Object()
const std::string &gStr_Substitution = EidosRegisteredString("Substitution", gID_Substitution);
const std::string &gStr_InteractionType = EidosRegisteredString("InteractionType", gID_InteractionType);
const std::string &gStr_SLiMgui = EidosRegisteredString("SLiMgui", gID_SLiMgui);

// strings for LogFile
const std::string &gStr_createLogFile = EidosRegisteredString("createLogFile", gID_createLogFile);
const std::string &gStr_logFiles = EidosRegisteredString("logFiles", gID_logFiles);
const std::string &gStr_LogFile = EidosRegisteredString("LogFile", gID_LogFile);
const std::string &gStr_logInterval = EidosRegisteredString("logInterval", gID_logInterval);
const std::string &gStr_precision = EidosRegisteredString("precision", gID_precision);
const std::string &gStr_addCustomColumn = EidosRegisteredString("addCustomColumn", gID_addCustomColumn);
const std::string &gStr_addCycle = EidosRegisteredString("addCycle", gID_addCycle);
const std::string &gStr_addCycleStage = EidosRegisteredString("addCycleStage", gID_addCycleStage);
const std::string &gStr_addMeanSDColumns = EidosRegisteredString("addMeanSDColumns", gID_addMeanSDColumns);
const std::string &gStr_addPopulationSexRatio = EidosRegisteredString("addPopulationSexRatio", gID_addPopulationSexRatio);
const std::string &gStr_addPopulationSize = EidosRegisteredString("addPopulationSize", gID_addPopulationSize);
const std::string &gStr_addSubpopulationSexRatio = EidosRegisteredString("addSubpopulationSexRatio", gID_addSubpopulationSexRatio);
const std::string &gStr_addSubpopulationSize = EidosRegisteredString("addSubpopulationSize", gID_addSubpopulationSize);
const std::string &gStr_addSuppliedColumn = EidosRegisteredString("addSuppliedColumn", gID_addSuppliedColumn);
const std::string &gStr_addTick = EidosRegisteredString("addTick", gID_addTick);
const std::string &gStr_flush = EidosRegisteredString("flush", gID_flush);
const std::string &gStr_logRow = EidosRegisteredString("logRow", gID_logRow);
const std::string &gStr_setLogInterval = EidosRegisteredString("setLogInterval", gID_setLogInterval);
const std::string &gStr_setFilePath = EidosRegisteredString("setFilePath", gID_setFilePath);
const std::string &gStr_setSuppliedValue = EidosRegisteredString("setSuppliedValue", gID_setSuppliedValue);
const std::string &gStr_willAutolog = EidosRegisteredString("willAutolog", gID_willAutolog);
const std::string &gStr_context = EidosRegisteredString("context", gID_context);

// mostly other fixed strings
const std::string &gStr_A = EidosRegisteredString("A", gID_A);
const std::string gStr_C = "C";	// these nucleotide strings are not registered, no need
const std::string gStr_G = "G";
const std::string gStr_T = "T";
const std::string &gStr_X = EidosRegisteredString("X", gID_X);
const std::string &gStr_Y = EidosRegisteredString("Y", gID_Y);
const std::string &gStr_f = EidosRegisteredString("f", gID_f);
const std::string &gStr_g = EidosRegisteredString("g", gID_g);
const std::string &gStr_e = EidosRegisteredString("e", gID_e);
//const std::string &gStr_n = EidosRegisteredString("n", gID_n);		now gEidosStr_n
const std::string &gStr_w = EidosRegisteredString("w", gID_w);
const std::string &gStr_l = EidosRegisteredString("l", gID_l);
const std::string &gStr_p = EidosRegisteredString("p", gID_p);
//const std::string &gStr_s = EidosRegisteredString("s", gID_s);		now gEidosStr_s
const std::string &gStr_species = EidosRegisteredString("species", gID_species);
const std::string &gStr_ticks = EidosRegisteredString("ticks", gID_ticks);
const std::string &gStr_speciesSpec = EidosRegisteredString("speciesSpec", gID_speciesSpec);
const std::string &gStr_ticksSpec = EidosRegisteredString("ticksSpec", gID_ticksSpec);
const std::string &gStr_first = EidosRegisteredString("first", gID_first);
const std::string &gStr_early = EidosRegisteredString("early", gID_early);
const std::string &gStr_late = EidosRegisteredString("late", gID_late);
const std::string &gStr_initialize = EidosRegisteredString("initialize", gID_initialize);
const std::string &gStr_fitnessEffect = EidosRegisteredString("fitnessEffect", gID_fitnessEffect);
const std::string &gStr_mutationEffect = EidosRegisteredString("mutationEffect", gID_mutationEffect);
const std::string &gStr_interaction = EidosRegisteredString("interaction", gID_interaction);
const std::string &gStr_mateChoice = EidosRegisteredString("mateChoice", gID_mateChoice);
const std::string &gStr_modifyChild = EidosRegisteredString("modifyChild", gID_modifyChild);
const std::string &gStr_recombination = EidosRegisteredString("recombination", gID_recombination);
const std::string &gStr_mutation = EidosRegisteredString("mutation", gID_mutation);
const std::string &gStr_survival = EidosRegisteredString("survival", gID_survival);
const std::string &gStr_reproduction = EidosRegisteredString("reproduction", gID_reproduction);


void SLiM_ConfigureContext(void)
{
	static bool been_here = false;
	
	THREAD_SAFETY_IN_ANY_PARALLEL("SLiM_ConfigureContext(): not warmed up");
	
	if (!been_here)
	{
		been_here = true;
		
		gEidosContextVersion = SLIM_VERSION_FLOAT;
		gEidosContextVersionString = std::string("SLiM version ") + std::string(SLIM_VERSION_STRING);
		gEidosContextLicense = "SLiM is free software: you can redistribute it and/or\nmodify it under the terms of the GNU General Public\nLicense as published by the Free Software Foundation,\neither version 3 of the License, or (at your option)\nany later version.\n\nSLiM is distributed in the hope that it will be\nuseful, but WITHOUT ANY WARRANTY; without even the\nimplied warranty of MERCHANTABILITY or FITNESS FOR\nA PARTICULAR PURPOSE.  See the GNU General Public\nLicense for more details.\n\nYou should have received a copy of the GNU General\nPublic License along with SLiM.  If not, see\n<http://www.gnu.org/licenses/>.\n";
		gEidosContextCitation = "To cite SLiM in publications please use:\n\nHaller, B.C., and Messer, P.W. (2023). SLiM 4:\nMultispecies eco-evolutionary modeling. The American\nNaturalist 201(5). DOI: https://doi.org/10.1086/723601\n\nFor papers using tree-sequence recording, please cite:\n\nHaller, B.C., Galloway, J., Kelleher, J., Messer, P.W.,\n& Ralph, P.L. (2019). Tree‐sequence recording in SLiM\nopens new horizons for forward‐time simulation of whole\ngenomes. Molecular Ecology Resources 19(2), 552-566.\nDOI: https://doi.org/10.1111/1755-0998.12968\n";
	}
}


// *************************************
//
//	TSKIT/tree sequence tables related
//
#pragma mark -
#pragma mark Tree sequences
#pragma mark -

// Metadata schemas:
// These should be valid json strings, parseable by python's json.loads( )
// and then turned into a valid metadata schema by tskit.MetadataSchema( ).
// You can check these by doing, in python:
// ```
// t = ( <paste in everything below except final semicolon> )
// d = json.loads(t)
// m = tskit.MetadataSchema(d)
// for e in d['examples']:
//    m.encode_row(e)
// ```
// Furthermore, so that they match with the way python would do it,
// we've produced these by doing :
// ```
// import pyslim
// for ms in pyslim.slim_metadata_schemas:
//   print(ms)
//   print(str(pyslim.slim_metadata_schemas[ms]))
// ```
// See the pyslim code for readable versions of these.

// For more info on schemas in tskit, see: https://tskit.dev/tskit/docs/stable/metadata.html#sec-metadata
 
// BCH 11/7/2021: Since I have been needing to modify these here by hand, I have changed them into C++ raw string literals.
// I have also added some code in SLiM_WarmUp() that checks that the metadata schemas are all valid JSON, and optionally prints them.
// see https://stackoverflow.com/a/5460235/2752221

const std::string gSLiM_tsk_metadata_schema =
R"V0G0N({"$schema":"http://json-schema.org/schema#","codec":"json","examples":[{"SLiM":{"file_version":"0.8","name":"fox","description":"foxes on Catalina island","cycle":123,"tick":123,"model_type":"WF","nucleotide_based":false,"separate_sexes":true,"spatial_dimensionality":"xy","spatial_periodicity":"x"}}],"properties":{"SLiM":{"description":"Top-level metadata for a SLiM tree sequence, file format version 0.8","properties":{"file_version":{"description":"The SLiM 'file format version' of this tree sequence.","type":"string"},"name":{"description":"The SLiM species name represented by this tree sequence.","type":"string"},"description":{"description":"A user-configurable description of the species represented by this tree sequence.","type":"string"},"cycle":{"description":"The 'SLiM cycle' counter when this tree sequence was recorded.","type":"integer"},"tick":{"description":"The 'SLiM tick' counter when this tree sequence was recorded.","type":"integer"},"model_type":{"description":"The model type used for the last part of this simulation (WF or nonWF).","enum":["WF","nonWF"],"type":"string"},"nucleotide_based":{"description":"Whether the simulation was nucleotide-based.","type":"boolean"},"separate_sexes":{"description":"Whether the simulation had separate sexes.","type":"boolean"},"spatial_dimensionality":{"description":"The spatial dimensionality of the simulation.","enum":["","x","xy","xyz"],"type":"string"},"spatial_periodicity":{"description":"The spatial periodicity of the simulation.","enum":["","x","y","z","xy","xz","yz","xyz"],"type":"string"},"stage":{"description":"The stage of the SLiM life cycle when this tree sequence was recorded.","type":"string"}},"required":["model_type","tick","file_version","spatial_dimensionality","spatial_periodicity","separate_sexes","nucleotide_based"],"type":"object"}},"required":["SLiM"],"type":"object"})V0G0N";

const std::string gSLiM_tsk_edge_metadata_schema = "";
const std::string gSLiM_tsk_site_metadata_schema = "";

const std::string gSLiM_tsk_mutation_metadata_schema =
R"V0G0N({"$schema":"http://json-schema.org/schema#","additionalProperties":false,"codec":"struct","description":"SLiM schema for mutation metadata.","examples":[{"mutation_list":[{"mutation_type":1,"nucleotide":3,"selection_coeff":-0.2,"slim_time":243,"subpopulation":0}]}],"properties":{"mutation_list":{"items":{"additionalProperties":false,"properties":{"mutation_type":{"binaryFormat":"i","description":"The index of this mutation's mutationType.","index":1,"type":"integer"},"nucleotide":{"binaryFormat":"b","description":"The nucleotide for this mutation (0=A , 1=C , 2=G, 3=T, or -1 for none)","index":5,"type":"integer"},"selection_coeff":{"binaryFormat":"f","description":"This mutation's selection coefficient.","index":2,"type":"number"},"slim_time":{"binaryFormat":"i","description":"The SLiM tick counter when this mutation occurred.","index":4,"type":"integer"},"subpopulation":{"binaryFormat":"i","description":"The ID of the subpopulation this mutation occurred in.","index":3,"type":"integer"}},"required":["mutation_type","selection_coeff","subpopulation","slim_time","nucleotide"],"type":"object"},"noLengthEncodingExhaustBuffer":true,"type":"array"}},"required":["mutation_list"],"type":"object"})V0G0N";

const std::string gSLiM_tsk_node_metadata_schema =
R"V0G0N({"$schema":"http://json-schema.org/schema#","additionalProperties":false,"codec":"struct","description":"SLiM schema for node metadata.","examples":[{"genome_type":0,"is_null":false,"slim_id":123}],"properties":{"genome_type":{"binaryFormat":"B","description":"The 'type' of this genome (0 for autosome, 1 for X, 2 for Y).","index":2,"type":"integer"},"is_null":{"binaryFormat":"?","description":"Whether this node describes a 'null' (non-existant) chromosome.","index":1,"type":"boolean"},"slim_id":{"binaryFormat":"q","description":"The 'pedigree ID' of this chromosome in SLiM.","index":0,"type":"integer"}},"required":["slim_id","is_null","genome_type"],"type":["object","null"]})V0G0N";

const std::string gSLiM_tsk_individual_metadata_schema =
R"V0G0N({"$schema":"http://json-schema.org/schema#","additionalProperties":false,"codec":"struct","description":"SLiM schema for individual metadata.","examples":[{"age":-1,"flags":0,"pedigree_id":123,"pedigree_p1":12,"pedigree_p2":23,"sex":0,"subpopulation":0}],"flags":{"SLIM_INDIVIDUAL_METADATA_MIGRATED":{"description":"Whether this individual was a migrant, either in the tick when the tree sequence was written out (if the individual was alive then), or in the tick of the last time they were Remembered (if not).","value":1}},"properties":{"age":{"binaryFormat":"i","description":"The age of this individual, either when the tree sequence was written out (if the individual was alive then), or the last time they were Remembered (if not).","index":4,"type":"integer"},"flags":{"binaryFormat":"I","description":"Other information about the individual: see 'flags'.","index":7,"type":"integer"},"pedigree_id":{"binaryFormat":"q","description":"The 'pedigree ID' of this individual in SLiM.","index":1,"type":"integer"},"pedigree_p1":{"binaryFormat":"q","description":"The 'pedigree ID' of this individual's first parent in SLiM.","index":2,"type":"integer"},"pedigree_p2":{"binaryFormat":"q","description":"The 'pedigree ID' of this individual's second parent in SLiM.","index":3,"type":"integer"},"sex":{"binaryFormat":"i","description":"The sex of the individual (0 for female, 1 for male, -1 for hermaphrodite).","index":6,"type":"integer"},"subpopulation":{"binaryFormat":"i","description":"The ID of the subpopulation the individual was part of, either when the tree sequence was written out (if the individual was alive then), or the last time they were Remembered (if not).","index":5,"type":"integer"}},"required":["pedigree_id","pedigree_p1","pedigree_p2","age","subpopulation","sex","flags"],"type":"object"})V0G0N";

// This schema was obsoleted in SLiM 3.7; we now use a JSON schema for the population metadata (see below)
const std::string gSLiM_tsk_population_metadata_schema_PREJSON = 
R"V0G0N({"$schema":"http://json-schema.org/schema#","additionalProperties":false,"codec":"struct","description":"SLiM schema for population metadata.","examples":[{"bounds_x0":0.0,"bounds_x1":100.0,"bounds_y0":0.0,"bounds_y1":100.0,"bounds_z0":0.0,"bounds_z1":100.0,"female_cloning_fraction":0.25,"male_cloning_fraction":0.0,"migration_records":[{"migration_rate":0.9,"source_subpop":1},{"migration_rate":0.1,"source_subpop":2}],"selfing_fraction":0.5,"sex_ratio":0.5,"slim_id":2}],"properties":{"bounds_x0":{"binaryFormat":"d","description":"The minimum x-coordinate in this subpopulation.","index":6,"type":"number"},"bounds_x1":{"binaryFormat":"d","description":"The maximum x-coordinate in this subpopulation.","index":7,"type":"number"},"bounds_y0":{"binaryFormat":"d","description":"The minimum y-coordinate in this subpopulation.","index":8,"type":"number"},"bounds_y1":{"binaryFormat":"d","description":"The maximum y-coordinate in this subpopulation.","index":9,"type":"number"},"bounds_z0":{"binaryFormat":"d","description":"The minimum z-coordinate in this subpopulation.","index":10,"type":"number"},"bounds_z1":{"binaryFormat":"d","description":"The maximum z-coordinate in this subpopulation.","index":11,"type":"number"},"female_cloning_fraction":{"binaryFormat":"d","description":"The frequency with which females in this subpopulation reproduce clonally (for WF models).","index":3,"type":"number"},"male_cloning_fraction":{"binaryFormat":"d","description":"The frequency with which males in this subpopulation reproduce clonally (for WF models).","index":4,"type":"number"},"migration_records":{"arrayLengthFormat":"I","index":13,"items":{"additionalProperties":false,"properties":{"migration_rate":{"binaryFormat":"d","description":"The fraction of children in this subpopulation that are composed of 'migrants' from the source subpopulation (in WF models).","index":2,"type":"number"},"source_subpop":{"binaryFormat":"i","description":"The ID of the subpopulation migrants come from (in WF models).","index":1,"type":"integer"}},"required":["source_subpop","migration_rate"],"type":"object"},"type":"array"},"selfing_fraction":{"binaryFormat":"d","description":"The frequency with which individuals in this subpopulation self (for WF models).","index":2,"type":"number"},"sex_ratio":{"binaryFormat":"d","description":"This subpopulation's sex ratio (for WF models).","index":5,"type":"number"},"slim_id":{"binaryFormat":"i","description":"The ID of this population in SLiM. Note that this is called a 'subpopulation' in SLiM.","index":1,"type":"integer"}},"required":["slim_id","selfing_fraction","female_cloning_fraction","male_cloning_fraction","sex_ratio","bounds_x0","bounds_x1","bounds_y0","bounds_y1","bounds_z0","bounds_z1","migration_records"],"type":["object","null"]})V0G0N";

// BCH 19 May 2022: removed the `required` status for the "slim_id" key for SLiM 4, to allow "carryover" metadata
// to validate without errors (but for it to be considered SLiM metadata, "slim_id" must nevertheless be present).
// This is a change from SLiM 3.7 (and before SLiM 3.7 we were pre-JSON), but we don't need to have the SLiM 3.7
// schema in SLiM since we don't check/validate schemas anyway in SLiM; we will just write out this new schema in
// SLiM 4, and on read we won't care whether the schema is the 3.7 or the 4.0 schema.
const std::string gSLiM_tsk_population_metadata_schema = 
R"V0G0N({"$schema":"http://json-schema.org/schema#","additionalProperties":true,"codec":"json","description":"SLiM schema for population metadata.","examples":[{"bounds_x0":0.0,"bounds_x1":100.0,"bounds_y0":0.0,"bounds_y1":100.0,"female_cloning_fraction":0.25,"male_cloning_fraction":0.0,"migration_records":[{"migration_rate":0.9,"source_subpop":1},{"migration_rate":0.1,"source_subpop":2}],"selfing_fraction":0.5,"sex_ratio":0.5,"slim_id":2,"name":"p2"}],"properties":{"bounds_x0":{"description":"The minimum x-coordinate in this subpopulation.","type":"number"},"bounds_x1":{"description":"The maximum x-coordinate in this subpopulation.","type":"number"},"bounds_y0":{"description":"The minimum y-coordinate in this subpopulation.","type":"number"},"bounds_y1":{"description":"The maximum y-coordinate in this subpopulation.","type":"number"},"bounds_z0":{"description":"The minimum z-coordinate in this subpopulation.","type":"number"},"bounds_z1":{"description":"The maximum z-coordinate in this subpopulation.","type":"number"},"description":{"description":"A description of this subpopulation.","type":"string"},"female_cloning_fraction":{"description":"The frequency with which females in this subpopulation reproduce clonally (for WF models).","type":"number"},"male_cloning_fraction":{"description":"The frequency with which males in this subpopulation reproduce clonally (for WF models).","type":"number"},"migration_records":{"items":{"properties":{"migration_rate":{"description":"The fraction of children in this subpopulation that are composed of 'migrants' from the source subpopulation (in WF models).","type":"number"},"source_subpop":{"description":"The ID of the subpopulation migrants come from (in WF models).","type":"integer"}},"required":["source_subpop","migration_rate"],"type":"object"},"type":"array"},"name":{"description":"A human-readable name for this subpopulation.","type":"string"},"selfing_fraction":{"description":"The frequency with which individuals in this subpopulation self (for WF models).","type":"number"},"sex_ratio":{"description":"This subpopulation's sex ratio (for WF models).","type":"number"},"slim_id":{"description":"The ID of this population in SLiM. Note that this is called a 'subpopulation' in SLiM.","type":"integer"}},"required":[],"type":["object","null"]})V0G0N";


// *************************************
//
//	Profiling
//
#pragma mark -
#pragma mark Profiling
#pragma mark -

#if (SLIMPROFILING == 1)
//
//	Support for profiling at the command line, and emitting the profile report in HTML
//	This is very parallel to the profile reporting code in SLiMguiLegacy and QtSLiM,
//	but without Cocoa or Qt, and emitting HTML instead of widget kit formats
//

#if DEBUG
#error In order to obtain accurate timing information that is relevant to the actual runtime of a model, profiling requires that you are running a Release build of SLiM.
#endif

static std::string StringForByteCount(int64_t bytes)
{
	char buf[128];		// used for printf-style formatted strings
	
	if (bytes > 512LL * 1024L * 1024L * 1024L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0));
		return std::string(buf).append(" TB");
	}
	else if (bytes > 512L * 1024L * 1024L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0 * 1024.0 * 1024.0));
		return std::string(buf).append(" GB");
	}
	else if (bytes > 512L * 1024L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0 * 1024.0));
		return std::string(buf).append(" MB");
	}
	else if (bytes > 512L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0));
		return std::string(buf).append(" KB");
	}
	else
	{
		snprintf(buf, 128, "%lld", (long long int)bytes);
		return std::string(buf).append(" bytes");
	}
}

#define SLIM_YELLOW_FRACTION 0.10
#define SLIM_SATURATION 0.75

static std::string SpanTagForColorFraction(double color_fraction)
{
	double r, g, b;
	char buf[256];
	
	if (color_fraction < SLIM_YELLOW_FRACTION)
	{
		// small fractions fall on a ramp from white (0.0) to yellow (SLIM_YELLOW_FRACTION)
		r = 1.0;
		g = 1.0;
		b = 1.0 - (color_fraction / SLIM_YELLOW_FRACTION) * SLIM_SATURATION;
	}
	else
	{
		// larger fractions ramp from yellow (SLIM_YELLOW_FRACTION) to red (1.0)
		r = 1.0;
		g = (1.0 - (color_fraction - SLIM_YELLOW_FRACTION) / (1.0 - SLIM_YELLOW_FRACTION));
		b = 0.0;
	}
	
	snprintf(buf, 256, "<span style='background-color:rgb(%d,%d,%d);'>", (int)round(r * 255), (int)round(g * 255), (int)round(b * 255));
	return std::string(buf);
}

static void _ColorScriptWithProfileCounts(const EidosASTNode *node, double elapsedTime, int32_t baseIndex, double *color_fractions)
{
	eidos_profile_t count = node->profile_total_;
	
	if (count > 0)
	{
		int32_t start = 0, end = 0;
		
		node->FullUTF8Range(&start, &end);
		
		start -= baseIndex;
		end -= baseIndex;
		
		double color_fraction = Eidos_ElapsedProfileTime(count) / elapsedTime;
		
		for (int32_t pos = start; pos <= end; pos++)
			color_fractions[pos] = color_fraction;
	}
	
	// Then let child nodes color
	for (const EidosASTNode *child : node->children_)
		_ColorScriptWithProfileCounts(child, elapsedTime, baseIndex, color_fractions);
}

static std::string ColorScriptWithProfileCounts(std::string script, const EidosASTNode *node, double elapsedTime, int32_t baseIndex)
{
	const char *script_data = script.data();
	size_t script_len = script.length();
	double *color_fractions = (double *)calloc(script_len, sizeof(double));
	
	// Color our range, and all children's ranges recursively
	_ColorScriptWithProfileCounts(node, elapsedTime, baseIndex, color_fractions);
	
	// Assemble our result string
	std::string result;
	double last_fraction = -1.0;
	
	for (size_t pos = 0; pos < script_len; ++pos)
	{
		// change the color if necessary
		if (color_fractions[pos] != last_fraction)
		{
			if (pos > 0)
				result.append("</span>");
			
			last_fraction = color_fractions[pos];
			result.append(SpanTagForColorFraction(last_fraction));
		}
		
		// emit the character, escaped as needed
		char ch = script_data[pos];
		
		if (ch == '&') result.append("&amp;");
		else if (ch == '<') result.append("&lt;");
		else if (ch == '>') result.append("&gt;");
		else if (ch == '\n') result.append("<BR>\n");
		else if (ch == '\r') ;
		else if (ch == '\t') result.append("&nbsp;&nbsp;&nbsp;");
		else result.append(1, ch);
	}
	
	if (script_len > 0)
		result.append("</span>");
	
	free(color_fractions);
	return result;
}

static std::string ColoredSpanForByteCount(int64_t bytes, double total)
{
	std::string byteString = StringForByteCount(bytes);
	double fraction = bytes / total;
	std::string spanTag = SpanTagForColorFraction(fraction);
	
	spanTag.append(byteString).append("</span>");
	return spanTag;
}

static std::string HTMLEncodeString(const std::string &data)
{
	// This is used to escape characters that have a special meaning in HTML
	std::string buffer;
	buffer.reserve(data.size());
	
	for (size_t pos = 0; pos != data.size(); ++pos)
	{
		switch (data[pos])
		{
			case '&':	buffer.append("&amp;");       			break;
			case '<':	buffer.append("&lt;");        			break;
			case '>':	buffer.append("&gt;");        			break;
			case '\n':	buffer.append("<BR>\n");				break;
			case '\r':											break;
			case '\t':	buffer.append("&nbsp;&nbsp;&nbsp;");	break;
			default:	buffer.append(&data[pos], 1); break;			// note we specify UTF-8 encoding, so this works with everything
		}
	}
	
	return buffer;
}

static std::string HTMLMakeSpacesNonBreaking(const char *data)
{
	// This is used to convert spaces ni &nbsp;, to make the alignment and formatting work properly
	std::string buffer;
	size_t pos = 0;
	
	while (true)
	{
		char ch = data[pos];
		
		if (ch == 0)				break;
		else if (data[pos] == ' ')	buffer.append("&nbsp;");
		else						buffer.append(&data[pos], 1);
		
		pos++;
	}
	
	return buffer;
}

void WriteProfileResults(std::string profile_output_path, std::string model_name, Community *community)
{
	std::string resolved_path = Eidos_ResolvedPath(profile_output_path);
	std::ofstream fout(resolved_path);
	char buf[256];		// used for printf-style formatted strings
	
	if (!fout.is_open())
		EIDOS_TERMINATION << std::endl << "ERROR (WriteProfileResults): Could not open profile output path " << profile_output_path << EidosTerminate();
	
	//
	//	HTML header
	//
	
	fout << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">" << "\n";
	fout << "<html>" << "\n";
	fout << "<head>" << "\n";
	fout << "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>" << "\n";
	fout << "<meta http-equiv='Content-Style-Type' content='text/css'>" << "\n";
	fout << "<title>Profile Report</title>" << "\n";
	fout << "<meta name='Generator' content='SLiM'>" << "\n";
	fout << "<style type='text/css'>" << "\n";
	fout << "    body {font-family: Optima,sans-serif; font-size: 14;}" << "\n";
	fout << "    h2 {margin-bottom: 0px;}" << "\n";
	fout << "    h3 {margin-bottom: 0px;}" << "\n";
	fout << "    p {margin-top: 5px;}" << "\n";
	fout << "    tt {font-family: Menlo,monospace; font-size: 13;}" << "\n";
	fout << "</style>" << "\n";
	fout << "</head>" << "\n\n";
	fout << "<body>" << "\n";
	
	
	//
	//	Profile header
	//
	
	double elapsedWallClockTime = (std::chrono::duration_cast<std::chrono::microseconds>(community->profile_end_clock - community->profile_start_clock).count()) / (double)1000000L;
	double elapsedCPUTimeInSLiM = community->profile_elapsed_CPU_clock / (double)CLOCKS_PER_SEC;
	double elapsedWallClockTimeInSLiM = Eidos_ElapsedProfileTime(community->profile_elapsed_wall_clock);
	slim_tick_t elapsedSLiMTicks = community->profile_end_tick - community->profile_start_tick;
	
	fout << "<h2>Profile Report" << "</h2>\n";
	fout << "<p>Model: " << model_name << "</p>\n\n";
	
	std::string start_date_string(ctime(&community->profile_start_date));
	std::string end_date_string(ctime(&community->profile_end_date));
	
	start_date_string.pop_back();		// remove the trailing newline that ctime() annoyingly adds
	end_date_string.pop_back();
	
	fout << "<p>Run start: " << start_date_string << "<BR>\n";
	fout << "Run end: " << end_date_string << "</p>\n\n";
	
#ifdef _OPENMP
	fout << "<p>Maximum parallel threads: " << gEidosMaxThreads << "</p>\n\n";
#endif
	
	snprintf(buf, 256, "%0.2f s", elapsedWallClockTime);
	fout << "<p>Elapsed wall clock time: " << buf << "<BR>\n";
	
	snprintf(buf, 256, "%0.2f s", elapsedWallClockTimeInSLiM);
	fout << "Elapsed wall clock time inside SLiM core (corrected): " << buf << "<BR>\n";
	
	snprintf(buf, 256, "%0.2f", elapsedCPUTimeInSLiM);
	fout << "Elapsed CPU time inside SLiM core (uncorrected): " << buf << " s" << "<BR>\n";
	
	fout << "Elapsed ticks: " << elapsedSLiMTicks << ((community->profile_start_tick == 0) ? " (including initialize)" : "") << "</p>\n\n";
	
	
	snprintf(buf, 256, "%0.2f ticks (%0.4g s)", gEidos_ProfileOverheadTicks, gEidos_ProfileOverheadSeconds);
	fout << "<p>Profile block external overhead: " << buf << "<BR>\n";
	
	snprintf(buf, 256, "%0.2f ticks (%0.4g s)", gEidos_ProfileLagTicks, gEidos_ProfileLagSeconds);
	fout << "Profile block internal lag: " << buf << "</p>\n\n";
	
	size_t total_usage = community->profile_total_memory_usage_Community.totalMemoryUsage + community->profile_total_memory_usage_AllSpecies.totalMemoryUsage;
	size_t average_usage = total_usage / community->total_memory_tallies_;
	size_t last_usage = community->profile_last_memory_usage_Community.totalMemoryUsage + community->profile_last_memory_usage_AllSpecies.totalMemoryUsage;
	
	fout << "<p>Average tick SLiM memory use: " << StringForByteCount(average_usage) << "<BR>\n";
	fout << "Final tick SLiM memory use: " << StringForByteCount(last_usage) << "</p>\n\n";
	
	
	//
	//	Cycle stage breakdown
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		bool isWF = (community->ModelType() == SLiMModelType::kModelTypeWF);
		double elapsedStage0Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[0]);
		double elapsedStage1Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[1]);
		double elapsedStage2Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[2]);
		double elapsedStage3Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[3]);
		double elapsedStage4Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[4]);
		double elapsedStage5Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[5]);
		double elapsedStage6Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[6]);
		double elapsedStage7Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[7]);
		double elapsedStage8Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[8]);
		double percentStage0 = (elapsedStage0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage1 = (elapsedStage1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage2 = (elapsedStage2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage3 = (elapsedStage3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage4 = (elapsedStage4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage5 = (elapsedStage5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage6 = (elapsedStage6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage7 = (elapsedStage7Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage8 = (elapsedStage8Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4;
		
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage0Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage1Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage2Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage3Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage4Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage5Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage6Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage7Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage8Time));
		
		fout << "<h3>Cycle stage breakdown" << "</h3>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage0Time, percentStage0);
		fout << "<p>" << HTMLMakeSpacesNonBreaking(buf) << " : initialize() callback execution" << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage1Time, percentStage1);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 0 - first() event execution" : " : stage 0 - first() event execution") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage2Time, percentStage2);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 1 - early() event execution" : " : stage 1 - offspring generation") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage3Time, percentStage3);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 2 - offspring generation" : " : stage 2 - early() event execution") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage4Time, percentStage4);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 3 - bookkeeping (fixed mutation removal, etc.)" : " : stage 3 - fitness calculation") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage5Time, percentStage5);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 4 - generation swap" : " : stage 4 - viability/survival selection") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage6Time, percentStage6);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 5 - late() event execution" : " : stage 5 - bookkeeping (fixed mutation removal, etc.)") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage7Time, percentStage7);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 6 - fitness calculation" : " : stage 6 - late() event execution") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage8Time, percentStage8);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 7 - tree sequence auto-simplification" : " : stage 7 - tree sequence auto-simplification") << "</p>\n\n";
	}
	
	
	//
	//	Callback type breakdown
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		double elapsedTime_first = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventFirst]);
		double elapsedTime_early = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventEarly]);
		double elapsedTime_late = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventLate]);
		double elapsedTime_initialize = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosInitializeCallback]);
		double elapsedTime_mutationEffect = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMutationEffectCallback]);
		double elapsedTime_fitnessEffect = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosFitnessEffectCallback]);
		double elapsedTime_interaction = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosInteractionCallback]);
		double elapsedTime_matechoice = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMateChoiceCallback]);
		double elapsedTime_modifychild = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosModifyChildCallback]);
		double elapsedTime_recombination = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosRecombinationCallback]);
		double elapsedTime_mutation = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMutationCallback]);
		double elapsedTime_reproduction = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosReproductionCallback]);
		double elapsedTime_survival = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosSurvivalCallback]);
		double percent_first = (elapsedTime_first / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_early = (elapsedTime_early / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_late = (elapsedTime_late / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_initialize = (elapsedTime_initialize / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_fitness = (elapsedTime_mutationEffect / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_fitnessglobal = (elapsedTime_fitnessEffect / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_interaction = (elapsedTime_interaction / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_matechoice = (elapsedTime_matechoice / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_modifychild = (elapsedTime_modifychild / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_recombination = (elapsedTime_recombination / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_mutation = (elapsedTime_mutation / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_reproduction = (elapsedTime_reproduction / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_survival = (elapsedTime_survival / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4, fw2 = 4;
		
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_first));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_early));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_late));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_initialize));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_mutationEffect));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_fitnessEffect));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_interaction));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_matechoice));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_modifychild));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_recombination));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_mutation));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_reproduction));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_survival));
		
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_first));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_early));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_late));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_initialize));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_fitness));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_fitnessglobal));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_interaction));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_matechoice));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_modifychild));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_recombination));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_mutation));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_reproduction));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_survival));
		
		fout << "<h3>Callback type breakdown" << "</h3>\n";
		
		// Note these are out of numeric order, but in cycle stage order
		if (community->ModelType() == SLiMModelType::kModelTypeWF)
		{
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_initialize, fw2, percent_initialize);
			fout << "<p>" << HTMLMakeSpacesNonBreaking(buf) << " : initialize() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_first, fw2, percent_first);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : first() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_early, fw2, percent_early);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : early() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_matechoice, fw2, percent_matechoice);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mateChoice() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_recombination, fw2, percent_recombination);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : recombination() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutation, fw2, percent_mutation);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutation() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_modifychild, fw2, percent_modifychild);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : modifyChild() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_late, fw2, percent_late);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : late() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutationEffect, fw2, percent_fitness);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutationEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_fitnessEffect, fw2, percent_fitnessglobal);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : fitnessEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_interaction, fw2, percent_interaction);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : interaction() callbacks" << "</p>\n\n";
			
		}
		else
		{
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_initialize, fw2, percent_initialize);
			fout << "<p>" << HTMLMakeSpacesNonBreaking(buf) << " : initialize() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_first, fw2, percent_first);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : first() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_reproduction, fw2, percent_reproduction);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : reproduction() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_recombination, fw2, percent_recombination);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : recombination() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutation, fw2, percent_mutation);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutation() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_modifychild, fw2, percent_modifychild);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : modifyChild() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_early, fw2, percent_early);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : early() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutationEffect, fw2, percent_fitness);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutationEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_fitnessEffect, fw2, percent_fitnessglobal);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : fitnessEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_survival, fw2, percent_survival);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : survival() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_late, fw2, percent_late);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : late() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_interaction, fw2, percent_interaction);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : interaction() callbacks" << "</p>\n\n";
		}
	}
	
	
	//
	//	Script block profiles
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		{
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			
			// Convert the profile counts in all script blocks into self counts (excluding the counts of nodes below them)
			for (SLiMEidosBlock *script_block : script_blocks)
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
					script_block->root_node_->ConvertProfileTotalsToSelfCounts();
		}
		
		{
			fout << "<h3>Script block profiles (as a fraction of corrected wall clock time)" << "</h3>\n";
			
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			bool hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>";
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
		
		{
			fout << "<h3>Script block profiles (as a fraction of within-block wall clock time)" << "</h3>\n";
			
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			bool hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>";
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, total_block_time, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
	}
	
	
	//
	//	User-defined functions (if any)
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		EidosFunctionMap &function_map = community->FunctionMap();
		std::vector<const EidosFunctionSignature *> userDefinedFunctions;
		
		for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
		{
			const EidosFunctionSignature *signature = functionPairIter->second.get();
			
			if (signature->body_script_ && signature->user_defined_)
			{
				signature->body_script_->AST()->ConvertProfileTotalsToSelfCounts();
				userDefinedFunctions.emplace_back(signature);
			}
		}
		
		if (userDefinedFunctions.size())
		{
			fout << "<h3>User-defined functions (as a fraction of corrected wall clock time)" << "</h3>\n";
			
			bool hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					const std::string &&signature_string = signature->SignatureString();
					
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>" << HTMLEncodeString(signature_string) << "<BR>";
					
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
		
		if (userDefinedFunctions.size())
		{
			fout << "<h3>User-defined functions (as a fraction of within-block wall clock time)" << "</h3>\n";
			
			bool hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					const std::string &&signature_string = signature->SignatureString();
					
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>" << HTMLEncodeString(signature_string) << "<BR>";
					
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, total_block_time, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
	}
	
	
#if SLIM_USE_NONNEUTRAL_CACHES
	//
	//	MutationRun metrics, presented per Species
	//
	const std::vector<Species *> &all_species = community->AllSpecies();
	
	for (Species *focal_species : all_species)
	{
		fout << "<h3>MutationRun usage";
		if (all_species.size() > 1)
			fout << " (" << HTMLEncodeString(focal_species->avatar_) << " " << HTMLEncodeString(focal_species->name_) << ")";
		fout << "</h3>\n";
		
		if (!focal_species->HasGenetics())
		{
			fout << "<p><i>(omitted for no-genetics species)" << "</i></p>\n";
			continue;
		}
		
		{
			int64_t power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
			int64_t power_tallies_total = (int)focal_species->profile_mutcount_history_.size();
			
			for (int power = 0; power < 20; ++power)
				power_tallies[power] = 0;
			
			for (int32_t count : focal_species->profile_mutcount_history_)
			{
				int power = (int)round(log2(count));
				
				power_tallies[power]++;
			}
			
			fout << "<p>";
			bool first_line = true;
			
			for (int power = 0; power < 20; ++power)
			{
				if (power_tallies[power] > 0)
				{
					if (!first_line)
						fout << "<BR>\n";
					snprintf(buf, 256, "%6.2f%%", (power_tallies[power] / (double)power_tallies_total) * 100.0);
					fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of ticks : " << ((int)(round(pow(2.0, power)))) << " mutation runs per genome";
					first_line = false;
				}
			}
			
			fout << "</p>\n";
		}
		
		{
			int64_t regime_tallies[3];
			int64_t regime_tallies_total = (int)focal_species->profile_nonneutral_regime_history_.size();
			
			for (int regime = 0; regime < 3; ++regime)
				regime_tallies[regime] = 0;
			
			for (int32_t regime : focal_species->profile_nonneutral_regime_history_)
				if ((regime >= 1) && (regime <= 3))
					regime_tallies[regime - 1]++;
				else
					regime_tallies_total--;
			
			fout << "<p>";
			bool first_line = true;
			
			for (int regime = 0; regime < 3; ++regime)
			{
				if (!first_line)
					fout << "<BR>\n";
				snprintf(buf, 256, "%6.2f%%", (regime_tallies[regime] / (double)regime_tallies_total) * 100.0);
				fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of ticks : regime " << (regime + 1) << " (" << (regime == 0 ? "no mutationEffect() callbacks" : (regime == 1 ? "constant neutral mutationEffect() callbacks only" : "unpredictable mutationEffect() callbacks present")) << ")";
				first_line = false;
			}
			
			fout << "</p>\n";
		}
		
		fout << "<p><tt>" << focal_species->profile_mutation_total_usage_ << "</tt> mutations referenced, summed across all ticks<BR>\n";
		fout << "<tt>" << focal_species->profile_nonneutral_mutation_total_ << "</tt> mutations considered potentially nonneutral<BR>\n";
		snprintf(buf, 256, "%0.2f%%", ((focal_species->profile_mutation_total_usage_ - focal_species->profile_nonneutral_mutation_total_) / (double)focal_species->profile_mutation_total_usage_) * 100.0);
		fout << "<tt>" << buf << "</tt> of mutations excluded from fitness calculations<BR>\n";
		fout << "<tt>" << focal_species->profile_max_mutation_index_ << "</tt> maximum simultaneous mutations</p>\n";
		
		fout << "<p><tt>" << focal_species->profile_mutrun_total_usage_ << "</tt> mutation runs referenced, summed across all ticks<BR>\n";
		fout << "<tt>" << focal_species->profile_unique_mutrun_total_ << "</tt> unique mutation runs maintained among those<BR>\n";
		snprintf(buf, 256, "%6.2f%%", (focal_species->profile_mutrun_nonneutral_recache_total_ / (double)focal_species->profile_unique_mutrun_total_) * 100.0);
		fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of mutation run nonneutral caches rebuilt per tick<BR>\n";
		snprintf(buf, 256, "%6.2f%%", ((focal_species->profile_mutrun_total_usage_ - focal_species->profile_unique_mutrun_total_) / (double)focal_species->profile_mutrun_total_usage_) * 100.0);
		fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of mutation runs shared among genomes</p>\n\n";
	}
#endif
	
	//
	//	Memory usage metrics
	//
	
	{
		fout << "<h3>SLiM memory usage (average / final tick) " << "</h3>\n";
		
		SLiMMemoryUsage_Community &mem_tot_C = community->profile_total_memory_usage_Community;
		SLiMMemoryUsage_Species &mem_tot_S = community->profile_total_memory_usage_AllSpecies;
		SLiMMemoryUsage_Community &mem_last_C = community->profile_last_memory_usage_Community;
		SLiMMemoryUsage_Species &mem_last_S = community->profile_last_memory_usage_AllSpecies;
		int64_t div = community->total_memory_tallies_;
		double ddiv = community->total_memory_tallies_;
		double average_total = (mem_tot_C.totalMemoryUsage + mem_tot_S.totalMemoryUsage) / ddiv;
		double final_total = mem_last_C.totalMemoryUsage + mem_last_S.totalMemoryUsage;
		
		// Chromosome
		snprintf(buf, 256, "%0.2f", mem_tot_S.chromosomeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.chromosomeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeObjects, final_total) << "</tt> : Chromosome objects (" << buf << " / " << mem_last_S.chromosomeObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.chromosomeMutationRateMaps / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeMutationRateMaps, final_total) << "</tt> : mutation rate maps<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.chromosomeRecombinationRateMaps / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeRecombinationRateMaps, final_total) << "</tt> : recombination rate maps<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.chromosomeAncestralSequence / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeAncestralSequence, final_total) << "</tt> : ancestral nucleotides</p>\n\n";
		
		// Community
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_C.communityObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.communityObjects, final_total) << "</tt> : Community object</p>\n\n";
		
		// Genome
		snprintf(buf, 256, "%0.2f", mem_tot_S.genomeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.genomeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeObjects, final_total) << "</tt> : Genome objects (" << buf << " / " << mem_last_S.genomeObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.genomeExternalBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeExternalBuffers, final_total) << "</tt> : external MutationRun* buffers<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.genomeUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeUnusedPoolSpace, final_total) << "</tt> : unused pool space<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.genomeUnusedPoolBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeUnusedPoolBuffers, final_total) << "</tt> : unused pool buffers</p>\n\n";
		
		// GenomicElement
		snprintf(buf, 256, "%0.2f", mem_tot_S.genomicElementObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.genomicElementObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomicElementObjects, final_total) << "</tt> : GenomicElement objects (" << buf << " / " << mem_last_S.genomicElementObjects_count << ")</p>\n\n";
		
		// GenomicElementType
		snprintf(buf, 256, "%0.2f", mem_tot_S.genomicElementTypeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.genomicElementTypeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomicElementTypeObjects, final_total) << "</tt> : GenomicElementType objects (" << buf << " / " << mem_last_S.genomicElementTypeObjects_count << ")</p>\n\n";
		
		// Individual
		snprintf(buf, 256, "%0.2f", mem_tot_S.individualObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.individualObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.individualObjects, final_total) << "</tt> : Individual objects (" << buf << " / " << mem_last_S.individualObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.individualUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.individualUnusedPoolSpace, final_total) << "</tt> : unused pool space</p>\n\n";
		
		// InteractionType
		snprintf(buf, 256, "%0.2f", mem_tot_C.interactionTypeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_C.interactionTypeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypeObjects, final_total) << "</tt> : InteractionType objects (" << buf << " / " << mem_last_C.interactionTypeObjects_count << ")";
		
		if (mem_tot_C.interactionTypeObjects_count || mem_last_C.interactionTypeObjects_count)
		{
			fout << "<BR>\n";	// finish previous line with <BR>
			
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.interactionTypeKDTrees / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypeKDTrees, final_total) << "</tt> : k-d trees<BR>\n";
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.interactionTypePositionCaches / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypePositionCaches, final_total) << "</tt> : position caches<BR>\n";
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.interactionTypeSparseVectorPool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypeSparseVectorPool, final_total) << "</tt> : sparse arrays";
		}
		
		fout << "</p>\n\n";
		
		// Mutation
		snprintf(buf, 256, "%0.2f", mem_tot_S.mutationObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.mutationObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationObjects, final_total) << "</tt> : Mutation objects (" << buf << " / " << mem_last_S.mutationObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.mutationRefcountBuffer / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.mutationRefcountBuffer, final_total) << "</tt> : refcount buffer<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.mutationUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.mutationUnusedPoolSpace, final_total) << "</tt> : unused pool space</p>\n\n";
		
		// MutationRun
		snprintf(buf, 256, "%0.2f", mem_tot_S.mutationRunObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.mutationRunObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunObjects, final_total) << "</tt> : MutationRun objects (" << buf << " / " << mem_last_S.mutationRunObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.mutationRunExternalBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunExternalBuffers, final_total) << "</tt> : external MutationIndex buffers<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.mutationRunNonneutralCaches / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunNonneutralCaches, final_total) << "</tt> : nonneutral mutation caches<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.mutationRunUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunUnusedPoolSpace, final_total) << "</tt> : unused pool space<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.mutationRunUnusedPoolBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunUnusedPoolBuffers, final_total) << "</tt> : unused pool buffers</p>\n\n";
		
		// MutationType
		snprintf(buf, 256, "%0.2f", mem_tot_S.mutationTypeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.mutationTypeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationTypeObjects, final_total) << "</tt> : MutationType objects (" << buf << " / " << mem_last_S.mutationTypeObjects_count << ")</p>\n\n";
		
		// Species
		snprintf(buf, 256, "%0.2f", mem_tot_S.speciesObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.speciesObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.speciesObjects, final_total) << "</tt> : Species objects (" << buf << " / " << mem_last_S.speciesObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.speciesTreeSeqTables / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.speciesTreeSeqTables, final_total) << "</tt> : tree-sequence tables</p>\n\n";
		
		// Subpopulation
		snprintf(buf, 256, "%0.2f", mem_tot_S.subpopulationObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.subpopulationObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationObjects, final_total) << "</tt> : Subpopulation objects (" << buf << " / " << mem_last_S.subpopulationObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationFitnessCaches / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationFitnessCaches, final_total) << "</tt> : fitness caches<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationParentTables / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationParentTables, final_total) << "</tt> : parent tables<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationSpatialMaps / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationSpatialMaps, final_total) << "</tt> : spatial maps";
		
		if (mem_tot_S.subpopulationSpatialMapsDisplay || mem_last_S.subpopulationSpatialMapsDisplay)
		{
			fout << "<BR>\n";	// finish previous line with <BR>
			
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationSpatialMapsDisplay / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationSpatialMapsDisplay, final_total) << "</tt> : spatial map display (SLiMgui only)";
		}
		
		fout << "</p>\n\n";
		
		// Substitution
		snprintf(buf, 256, "%0.2f", mem_tot_S.substitutionObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.substitutionObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.substitutionObjects, final_total) << "</tt> : Substitution objects (" << buf << " / " << mem_last_S.substitutionObjects_count << ")</p>\n\n";
		
		// Eidos
		fout << "<p>Eidos:<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.eidosASTNodePool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.eidosASTNodePool, final_total) << "</tt> : EidosASTNode pool<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.eidosSymbolTablePool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.eidosSymbolTablePool, final_total) << "</tt> : EidosSymbolTable pool<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.eidosValuePool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.eidosValuePool, final_total) << "</tt> : EidosValue pool<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.fileBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.fileBuffers, final_total) << "</tt> : File buffers</p>\n";
	}
	
	
	//
	//	HTML footer
	//
	
	fout << "</body>" << "\n";
	fout << "</html>" << std::endl;
	
	fout.close();
	if (!fout)
		EIDOS_TERMINATION << std::endl << "ERROR (WriteProfileResults): Could not write to profile output path " << profile_output_path << EidosTerminate();
}
#endif






















































