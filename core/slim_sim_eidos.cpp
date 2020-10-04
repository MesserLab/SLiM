//
//  slim_sim_eidos.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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


#include "slim_sim.h"

#include "genome.h"
#include "individual.h"
#include "subpopulation.h"
#include "polymorphism.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <utility>
#include <algorithm>
#include <vector>
#include <cmath>


static void PrintBytes(std::ostream &p_out, size_t p_bytes)
{
	p_out << p_bytes << " bytes";
	
	if (p_bytes > 1024L * 1024L * 1024L * 1024L)
		p_out << " (" << (p_bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0)) << " TB" << ")";
	else if (p_bytes > 1024L * 1024L * 1024L)
		p_out << " (" << (p_bytes / (1024.0 * 1024.0 * 1024.0)) << " GB" << ")";
	else if (p_bytes > 1024L * 1024L)
		p_out << " (" << (p_bytes / (1024.0 * 1024.0)) << " MB" << ")";
	else if (p_bytes > 1024)
		p_out << " (" << (p_bytes / 1024.0) << " K" << ")";
	
	p_out << std::endl;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

EidosValue_SP SLiMSim::ContextDefinedFunctionDispatch(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused(p_interpreter)
	
	// we only define initialize...() functions; so we must be in an initialize() callback
	if (generation_ != 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ContextDefinedFunctionDispatch): the function " << p_function_name << "() may only be called in an initialize() callback." << EidosTerminate();
	
	if (p_function_name.compare(gStr_initializeAncestralNucleotides) == 0)		return ExecuteContextFunction_initializeAncestralNucleotides(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeGenomicElement) == 0)		return ExecuteContextFunction_initializeGenomicElement(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeGenomicElementType) == 0)	return ExecuteContextFunction_initializeGenomicElementType(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeInteractionType) == 0)		return ExecuteContextFunction_initializeInteractionType(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeMutationType) == 0)			return ExecuteContextFunction_initializeMutationType(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeMutationTypeNuc) == 0)		return ExecuteContextFunction_initializeMutationType(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeRecombinationRate) == 0)	return ExecuteContextFunction_initializeRecombinationRate(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeGeneConversion) == 0)		return ExecuteContextFunction_initializeGeneConversion(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeMutationRate) == 0)			return ExecuteContextFunction_initializeMutationRate(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeHotspotMap) == 0)			return ExecuteContextFunction_initializeHotspotMap(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeSex) == 0)					return ExecuteContextFunction_initializeSex(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeSLiMOptions) == 0)			return ExecuteContextFunction_initializeSLiMOptions(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeTreeSeq) == 0)				return ExecuteContextFunction_initializeTreeSeq(p_function_name, p_arguments, p_interpreter);
	else if (p_function_name.compare(gStr_initializeSLiMModelType) == 0)		return ExecuteContextFunction_initializeSLiMModelType(p_function_name, p_arguments, p_interpreter);
	
	EIDOS_TERMINATION << "ERROR (SLiMSim::ContextDefinedFunctionDispatch): the function " << p_function_name << "() is not implemented by SLiMSim." << EidosTerminate();
}

//	*********************	(integer$)initializeAncestralNucleotides(is sequence)
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *sequence_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_ancseq_declarations_ > 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): initializeAncestralNucleotides() may be called only once." << EidosTerminate();
	if (!nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): initializeAncestralNucleotides() may be only be called in nucleotide-based models." << EidosTerminate();
	
	EidosValueType sequence_value_type = sequence_value->Type();
	int sequence_value_count = sequence_value->Count();
	
	if (sequence_value_count == 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): initializeAncestralNucleotides() requires a sequence of length >= 1." << EidosTerminate();
	
	if (sequence_value_type == EidosValueType::kValueInt)
	{
		// A vector of integers has been provided, where ACGT == 0123
		if (sequence_value_count == 1)
		{
			// singleton case
			int64_t int_value = sequence_value->IntAtIndex(0, nullptr);
			
			chromosome_.ancestral_seq_buffer_ = new NucleotideArray(1);
			chromosome_.ancestral_seq_buffer_->SetNucleotideAtIndex((std::size_t)0, (uint64_t)int_value);
		}
		else
		{
			// non-singleton, direct access
			const EidosValue_Int_vector *int_vec = sequence_value->IntVector();
			const int64_t *int_data = int_vec->data();
			
			try {
				chromosome_.ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, int_data);
			} catch (...) {
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): integer nucleotide values must be 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			}
		}
	}
	else if (sequence_value_type == EidosValueType::kValueString)
	{
		if (sequence_value_count != 1)
		{
			// A vector of characters has been provided, which must all be "A" / "C" / "G" / "T"
			const std::vector<std::string> *string_vec = sequence_value->StringVector();
			
			try {
				chromosome_.ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, *string_vec);
			} catch (...) {
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): string nucleotide values must be 'A', 'C', 'G', or 'T'." << EidosTerminate();
			}
		}
		else	// sequence_value_count == 1
		{
			const std::string &sequence_string = sequence_value->IsSingleton() ? ((EidosValue_String_singleton *)sequence_value)->StringValue() : (*sequence_value->StringVector())[0];
			bool contains_only_nuc = true;
			
			try {
				chromosome_.ancestral_seq_buffer_ = new NucleotideArray(sequence_string.length(), sequence_string.c_str());
			} catch (...) {
				contains_only_nuc = false;
			}
			
			if (!contains_only_nuc)
			{
				// A singleton string has been provided that contains characters other than ACGT; we will interpret it as a filesystem path for a FASTA file
				std::string file_path = Eidos_ResolvedPath(sequence_string);
				std::ifstream file_stream(file_path.c_str());
				
				if (!file_stream.is_open())
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): the file at path " << sequence_string << " could not be opened or does not exist." << EidosTerminate();
				
				bool started_sequence = false;
				std::string line, fasta_sequence;
				
				while (getline(file_stream, line))
				{
					// skippable lines are blank or start with a '>' or ';'
					// we skip over them if they're at the start of the file; once we start a sequence, they terminate the sequence
					bool skippable = ((line.length() == 0) || (line[0] == '>') || (line[0] == ';'));
					
					if (!started_sequence && skippable)
						continue;
					if (skippable)
						break;
					
					// otherwise, append the nucleotides from this line, removing a \r if one is present at the end of the line
					if (line.back() == '\r')
						line.pop_back();
					
					fasta_sequence.append(line);
					started_sequence = true;
				}
				
				if (file_stream.bad())
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): a filesystem error occurred while reading the file at path " << sequence_string << "." << EidosTerminate();
				
				if (fasta_sequence.length() == 0)
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): no FASTA sequence found in " << sequence_string << "." << EidosTerminate();
				
				try {
					chromosome_.ancestral_seq_buffer_ = new NucleotideArray(fasta_sequence.length(), fasta_sequence.c_str());
				} catch (...) {
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeAncestralNucleotides): FASTA sequence data must contain only the nucleotides ACGT." << EidosTerminate();
				}
			}
		}
	}
	
	// debugging
	//std::cout << "ancestral sequence set: " << *chromosome_.ancestral_seq_buffer_ << std::endl;
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeAncestralNucleotides(\"";
		
		// output up to 20 nucleotides, followed by an ellipsis if necessary
		for (std::size_t i = 0; (i < 20) && (i < chromosome_.ancestral_seq_buffer_->size()); ++i)
			output_stream << "ACGT"[chromosome_.ancestral_seq_buffer_->NucleotideAtIndex(i)];
		
		if (chromosome_.ancestral_seq_buffer_->size() > 20)
			output_stream << gEidosStr_ELLIPSIS;
		
		output_stream << "\");" << std::endl;
	}
	
	num_ancseq_declarations_++;
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(chromosome_.ancestral_seq_buffer_->size()));
}

//	*********************	(object<GenomicElement>)initializeGenomicElement(io<GenomicElementType> genomicElementType, integer start, integer end)
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeGenomicElement(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *genomicElementType_value = p_arguments[0].get();
	EidosValue *start_value = p_arguments[1].get();
	EidosValue *end_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (start_value->Count() != end_value->Count())
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() requires start and end to be the same length." << EidosTerminate();
	if ((genomicElementType_value->Count() != 1) && (genomicElementType_value->Count() != start_value->Count()))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() requires genomicElementType to be a singleton, or to match the length of start and end." << EidosTerminate();
	
	int element_count = start_value->Count();
	int type_count = genomicElementType_value->Count();
	
	if (element_count == 0)
		return gStaticEidosValueVOID;
	
	GenomicElementType *genomic_element_type_ptr_0 = ((type_count == 1) ? SLiM_ExtractGenomicElementTypeFromEidosValue_io(genomicElementType_value, 0, *this, "initializeGenomicElement()") : nullptr);
	GenomicElementType *genomic_element_type_ptr = nullptr;
	slim_position_t start_position = 0, end_position = 0;
	EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_GenomicElement_Class))->resize_no_initialize(element_count);
	
	for (int element_index = 0; element_index < element_count; ++element_index)
	{
		genomic_element_type_ptr = ((type_count == 1) ? genomic_element_type_ptr_0 : SLiM_ExtractGenomicElementTypeFromEidosValue_io(genomicElementType_value, element_index, *this, "initializeGenomicElement()"));
		start_position = SLiMCastToPositionTypeOrRaise(start_value->IntAtIndex(element_index, nullptr));
		end_position = SLiMCastToPositionTypeOrRaise(end_value->IntAtIndex(element_index, nullptr));
		
		if (end_position < start_position)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() end position " << end_position << " is less than start position " << start_position << "." << EidosTerminate();
		
		// Check that the new element will not overlap any existing element; if end_position > last_genomic_element_position we are safe.
		// Otherwise, we have to check all previously defined elements.  The use of last_genomic_element_position is an optimization to
		// avoid an O(N) scan with each added element; as long as elements are added in sorted order there is no need to scan.
		if (start_position <= last_genomic_element_position_)
		{
			for (GenomicElement *element : chromosome_.GenomicElements())
			{
				if ((element->start_position_ <= end_position) && (element->end_position_ >= start_position))
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() genomic element from start position " << start_position << " to end position " << end_position << " overlaps existing genomic element." << EidosTerminate();
			}
		}
		
		if (end_position > last_genomic_element_position_)
			last_genomic_element_position_ = end_position;
		
		// Create and add the new element
		GenomicElement *new_genomic_element = new GenomicElement(genomic_element_type_ptr, start_position, end_position);
		
		chromosome_.GenomicElements().emplace_back(new_genomic_element);
		result_vec->set_object_element_no_check_NORR(new_genomic_element, element_index);
		
		chromosome_changed_ = true;
		num_genomic_elements_++;
	}
	
	if (SLiM_verbosity_level >= 1)
	{
		if (ABBREVIATE_DEBUG_INPUT && (num_genomic_elements_ > 20) && (num_genomic_elements_ != element_count))
		{
			if ((num_genomic_elements_ - element_count) <= 20)
				output_stream << "(...initializeGenomicElement() calls omitted...)" << std::endl;
		}
		else if (element_count == 1)
		{
			output_stream << "initializeGenomicElement(g" << genomic_element_type_ptr->genomic_element_type_id_ << ", " << start_position << ", " << end_position << ");" << std::endl;
		}
		else
		{
			output_stream << "initializeGenomicElement(...);" << std::endl;
		}
	}
	
	return EidosValue_SP(result_vec);
}

//	*********************	(object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions, [Nf mutationMatrix = NULL])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeGenomicElementType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *mutationTypes_value = p_arguments[1].get();
	EidosValue *proportions_value = p_arguments[2].get();
	EidosValue *mutationMatrix_value = p_arguments[3].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	slim_objectid_t map_identifier = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 'g');
	
	if (genomic_element_types_.count(map_identifier) > 0) 
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() genomic element type g" << map_identifier << " already defined." << EidosTerminate();
	
	int mut_type_id_count = mutationTypes_value->Count();
	int proportion_count = proportions_value->Count();
	
	if (mut_type_id_count != proportion_count)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() requires the sizes of mutationTypes and proportions to be equal." << EidosTerminate();
	
	std::vector<MutationType*> mutation_types;
	std::vector<double> mutation_fractions;
	
	for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
	{
		MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationTypes_value, mut_type_index, *this, "initializeGenomicElementType()");
		double proportion = proportions_value->FloatAtIndex(mut_type_index, nullptr);
		
		if ((proportion < 0) || !std::isfinite(proportion))		// == 0 is allowed but must be fixed before the simulation executes; see InitializeDraws()
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() proportions must be greater than or equal to zero (" << EidosStringForFloat(proportion) << " supplied)." << EidosTerminate();
		
		if (std::find(mutation_types.begin(), mutation_types.end(), mutation_type_ptr) != mutation_types.end())
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() mutation type m" << mutation_type_ptr->mutation_type_id_ << " used more than once." << EidosTerminate();
		
		if (nucleotide_based_ && !mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): in nucleotide-based models, initializeGenomicElementType() requires all mutation types for the genomic element type to be nucleotide-based.  Non-nucleotide-based mutation types may be used in nucleotide-based models, but they cannot be autogenerated by SLiM, and therefore cannot be referenced by a genomic element type." << EidosTerminate();
		
		mutation_types.emplace_back(mutation_type_ptr);
		mutation_fractions.emplace_back(proportion);
		
		// check whether we are using a mutation type that is non-neutral; check and set pure_neutral_
		if ((mutation_type_ptr->dfe_type_ != DFEType::kFixed) || (mutation_type_ptr->dfe_parameters_[0] != 0.0))
		{
			SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
			
			sim.pure_neutral_ = false;
			// the mutation type's all_pure_neutral_DFE_ flag is presumably already set
		}
	}
	
	EidosValueType mm_type = mutationMatrix_value->Type();
	
	if (!nucleotide_based_ && (mm_type != EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() requires mutationMatrix to be NULL in non-nucleotide-based models." << EidosTerminate();
	if (nucleotide_based_ && (mm_type == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() requires mutationMatrix to be non-NULL in nucleotide-based models." << EidosTerminate();
	
	GenomicElementType *new_genomic_element_type = new GenomicElementType(*this, map_identifier, mutation_types, mutation_fractions);
	if (nucleotide_based_)
		new_genomic_element_type->SetNucleotideMutationMatrix(EidosValue_Float_vector_SP((EidosValue_Float_vector *)(mutationMatrix_value)));
	
	genomic_element_types_.insert(std::pair<const slim_objectid_t,GenomicElementType*>(map_identifier, new_genomic_element_type));
	genomic_element_types_changed_ = true;
	
	// define a new Eidos variable to refer to the new genomic element type
	EidosSymbolTableEntry &symbol_entry = new_genomic_element_type->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	
	if (SLiM_verbosity_level >= 1)
	{
		if (ABBREVIATE_DEBUG_INPUT && (num_genomic_element_types_ > 99))
		{
			if (num_genomic_element_types_ == 100)
				output_stream << "(...more initializeGenomicElementType() calls omitted...)" << std::endl;
		}
		else
		{
			output_stream << "initializeGenomicElementType(" << map_identifier;
			
			output_stream << ((mut_type_id_count > 1) ? ", c(" : ", ");
			for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
				output_stream << (mut_type_index > 0 ? ", m" : "m") << mutation_types[mut_type_index]->mutation_type_id_;
			output_stream << ((mut_type_id_count > 1) ? ")" : "");
			
			output_stream << ((mut_type_id_count > 1) ? ", c(" : ", ");
			for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
				output_stream << (mut_type_index > 0 ? ", " : "") << proportions_value->FloatAtIndex(mut_type_index, nullptr);
			output_stream << ((mut_type_id_count > 1) ? ")" : "");
			
			output_stream << ");" << std::endl;
		}
	}
	
	num_genomic_element_types_++;
	return symbol_entry.second;
}

//	*********************	(object<InteractionType>$)initializeInteractionType(is$ id, string$ spatiality, [logical$ reciprocal = F], [numeric$ maxDistance = INF], [string$ sexSegregation = "**"])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeInteractionType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *spatiality_value = p_arguments[1].get();
	EidosValue *reciprocal_value = p_arguments[2].get();
	EidosValue *maxDistance_value = p_arguments[3].get();
	EidosValue *sexSegregation_value = p_arguments[4].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	slim_objectid_t map_identifier = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 'i');
	std::string spatiality_string = spatiality_value->StringAtIndex(0, nullptr);
	bool reciprocal = reciprocal_value->LogicalAtIndex(0, nullptr);
	double max_distance = maxDistance_value->FloatAtIndex(0, nullptr);
	std::string sex_string = sexSegregation_value->StringAtIndex(0, nullptr);
	int required_dimensionality;
	IndividualSex receiver_sex = IndividualSex::kUnspecified, exerter_sex = IndividualSex::kUnspecified;
	
	if (interaction_types_.count(map_identifier) > 0) 
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() interaction type m" << map_identifier << " already defined." << EidosTerminate();
	
	if (spatiality_string.length() == 0)					required_dimensionality = 0;
	else if (spatiality_string.compare(gEidosStr_x) == 0)	required_dimensionality = 1;
	else if (spatiality_string.compare(gEidosStr_y) == 0)	required_dimensionality = 2;
	else if (spatiality_string.compare(gEidosStr_z) == 0)	required_dimensionality = 3;
	else if (spatiality_string.compare("xy") == 0)			required_dimensionality = 2;
	else if (spatiality_string.compare("xz") == 0)			required_dimensionality = 3;
	else if (spatiality_string.compare("yz") == 0)			required_dimensionality = 3;
	else if (spatiality_string.compare("xyz") == 0)			required_dimensionality = 3;
	else
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() spatiality \"" << spatiality_string << "\" must be \"\", \"x\", \"y\", \"z\", \"xy\", \"xz\", \"yz\", or \"xyz\"." << EidosTerminate();
	
	if (required_dimensionality > spatial_dimensionality_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() spatiality cannot utilize spatial dimensions beyond those set in initializeSLiMOptions()." << EidosTerminate();
	
	if (max_distance < 0.0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() maxDistance must be >= 0.0." << EidosTerminate();
	if ((required_dimensionality == 0) && (!std::isinf(max_distance) || (max_distance < 0.0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() maxDistance must be INF for non-spatial interactions." << EidosTerminate();
	
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
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() unsupported sexSegregation value (must be '**', '*M', '*F', 'M*', 'MM', 'MF', 'F*', 'FM', or 'FF')." << EidosTerminate();
	
	if (((receiver_sex != IndividualSex::kUnspecified) || (exerter_sex != IndividualSex::kUnspecified)) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() sexSegregation value other than '**' unsupported in non-sexual simulation." << EidosTerminate();
	
	if ((required_dimensionality > 0) && std::isinf(max_distance))
	{
		if (!gEidosSuppressWarnings)
		{
			if (!warned_no_max_distance_)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() called to configure a spatial interaction type with no maximum distance; this may result in very poor performance." << std::endl;
				warned_no_max_distance_ = true;
			}
		}
	}
	
	InteractionType *new_interaction_type = new InteractionType(*this, map_identifier, spatiality_string, reciprocal, max_distance, receiver_sex, exerter_sex);
	
	interaction_types_.insert(std::pair<const slim_objectid_t,InteractionType*>(map_identifier, new_interaction_type));
	interaction_types_changed_ = true;
	
	// define a new Eidos variable to refer to the new mutation type
	EidosSymbolTableEntry &symbol_entry = new_interaction_type->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	
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

//	*********************	(object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
//	*********************	(object<MutationType>$)initializeMutationTypeNuc(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeMutationType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	// Figure out whether the mutation type is nucleotide-based
	bool nucleotide_based = (p_function_name == "initializeMutationTypeNuc");
	
	if (nucleotide_based && !nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationType): initializeMutationTypeNuc() may be only be called in nucleotide-based models." << EidosTerminate();
	
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *dominanceCoeff_value = p_arguments[1].get();
	EidosValue *distributionType_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	slim_objectid_t map_identifier = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 'm');
	double dominance_coeff = dominanceCoeff_value->FloatAtIndex(0, nullptr);
	std::string dfe_type_string = distributionType_value->StringAtIndex(0, nullptr);
	
	if (mutation_types_.count(map_identifier) > 0) 
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationType): " << p_function_name << "() mutation type m" << map_identifier << " already defined." << EidosTerminate();
	
	// Parse the DFE type and parameters, and do various sanity checks
	DFEType dfe_type;
	std::vector<double> dfe_parameters;
	std::vector<std::string> dfe_strings;
	
	MutationType::ParseDFEParameters(dfe_type_string, p_arguments.data() + 3, (int)p_arguments.size() - 3, &dfe_type, &dfe_parameters, &dfe_strings);
	
#ifdef SLIMGUI
	// each new mutation type gets a unique zero-based index, used by SLiMgui to categorize mutations
	MutationType *new_mutation_type = new MutationType(*this, map_identifier, dominance_coeff, nucleotide_based, dfe_type, dfe_parameters, dfe_strings, num_mutation_types_);
#else
	MutationType *new_mutation_type = new MutationType(*this, map_identifier, dominance_coeff, nucleotide_based, dfe_type, dfe_parameters, dfe_strings);
#endif
	
	mutation_types_.insert(std::pair<const slim_objectid_t,MutationType*>(map_identifier, new_mutation_type));
	mutation_types_changed_ = true;
	
	// define a new Eidos variable to refer to the new mutation type
	EidosSymbolTableEntry &symbol_entry = new_mutation_type->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationType): " << p_function_name << "() symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	
	if (SLiM_verbosity_level >= 1)
	{
		if (ABBREVIATE_DEBUG_INPUT && (num_mutation_types_ > 99))
		{
			if (num_mutation_types_ == 100)
				output_stream << "(...more " << p_function_name << "() calls omitted...)" << std::endl;
		}
		else
		{
			output_stream << p_function_name << "(" << map_identifier << ", " << dominance_coeff << ", \"" << dfe_type << "\"";
			
			if (dfe_parameters.size() > 0)
			{
				for (double dfe_param : dfe_parameters)
					output_stream << ", " << dfe_param;
			}
			else
			{
				for (std::string dfe_param : dfe_strings)
					output_stream << ", \"" << dfe_param << "\"";
			}
			
			output_stream << ");" << std::endl;
		}
	}
	
	num_mutation_types_++;
	return symbol_entry.second;
}

//	*********************	(void)initializeRecombinationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeRecombinationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex;
	std::string sex_string = sex_value->StringAtIndex(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requested sex \"" << sex_string << "\" unsupported." << EidosTerminate();
	
	if ((requested_sex != IndividualSex::kUnspecified) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() sex-specific recombination map supplied in non-sexual simulation." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state.  Since single_recombination_map_ has not been set
	// yet, we just look to see whether the chromosome's policy has already been determined or not.
	if (((requested_sex == IndividualSex::kUnspecified) && ((chromosome_.recombination_rates_M_.size() != 0) || (chromosome_.recombination_rates_F_.size() != 0))) ||
		((requested_sex != IndividualSex::kUnspecified) && (chromosome_.recombination_rates_H_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	if (((requested_sex == IndividualSex::kUnspecified) && (num_recombination_rates_ > 0)) || ((requested_sex != IndividualSex::kUnspecified) && (num_recombination_rates_ > 1)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() may be called only once (or once per sex, with sex-specific recombination maps).  The multiple recombination regions of a recombination map must be set up in a single call to initializeRecombinationRate()." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? chromosome_.recombination_end_positions_H_ : 
											   ((requested_sex == IndividualSex::kMale) ? chromosome_.recombination_end_positions_M_ : chromosome_.recombination_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? chromosome_.recombination_rates_H_ : 
								  ((requested_sex == IndividualSex::kMale) ? chromosome_.recombination_rates_M_ : chromosome_.recombination_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double recombination_rate = rates_value->FloatAtIndex(0, nullptr);
		
		// check values
		if ((recombination_rate < 0.0) || (recombination_rate > 0.5) || std::isnan(recombination_rate))
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires rates to be in [0.0, 0.5] (" << EidosStringForFloat(recombination_rate) << " supplied)." << EidosTerminate();
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		rates.emplace_back(recombination_rate);
		//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	else
	{
		int end_count = ends_value->Count();
		
		if ((end_count != rate_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double recombination_rate = rates_value->FloatAtIndex(value_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(value_index, nullptr));
			
			if (value_index > 0)
				if (recombination_end_position <= ends_value->IntAtIndex(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((recombination_rate < 0.0) || (recombination_rate > 0.5) || std::isnan(recombination_rate))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires rates to be in [0.0, 0.5] (" << EidosStringForFloat(recombination_rate) << " supplied)." << EidosTerminate();
		}
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double recombination_rate = rates_value->FloatAtIndex(interval_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(interval_index, nullptr));
			
			rates.emplace_back(recombination_rate);
			positions.emplace_back(recombination_end_position);
		}
	}
	
	chromosome_changed_ = true;
	
	if (SLiM_verbosity_level >= 1)
	{
		int ratesSize = (int)rates.size();
		int endsSize = (int)positions.size();
		
		output_stream << "initializeRecombinationRate(";
		
		if (ratesSize > 1)
			output_stream << "c(";
		for (int interval_index = 0; interval_index < ratesSize; ++interval_index)
		{
			if (interval_index >= 50)
			{
				output_stream << ", ...";
				break;
			}
			
			output_stream << (interval_index == 0 ? "" : ", ") << rates[interval_index];
		}
		if (ratesSize > 1)
			output_stream << ")";
		
		if (endsSize > 0)
		{
			output_stream << ", ";
			
			if (endsSize > 1)
				output_stream << "c(";
			for (int interval_index = 0; interval_index < endsSize; ++interval_index)
			{
				if (interval_index >= 50)
				{
					output_stream << ", ...";
					break;
				}
				
				output_stream << (interval_index == 0 ? "" : ", ") << positions[interval_index];
			}
			if (endsSize > 1)
				output_stream << ")";
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_recombination_rates_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeGeneConversion(numeric$ nonCrossoverFraction, numeric$ meanLength, numeric$ simpleConversionFraction, [numeric$ bias = 0])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeGeneConversion(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *nonCrossoverFraction_value = p_arguments[0].get();
	EidosValue *meanLength_value = p_arguments[1].get();
	EidosValue *simpleConversionFraction_value = p_arguments[2].get();
	EidosValue *bias_value = p_arguments[3].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_gene_conversions_ > 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() may be called only once." << EidosTerminate();
	
	double non_crossover_fraction = nonCrossoverFraction_value->FloatAtIndex(0, nullptr);
	double gene_conversion_avg_length = meanLength_value->FloatAtIndex(0, nullptr);
	double simple_conversion_fraction = simpleConversionFraction_value->FloatAtIndex(0, nullptr);
	double bias = bias_value->FloatAtIndex(0, nullptr);
	
	if ((non_crossover_fraction < 0.0) || (non_crossover_fraction > 1.0) || std::isnan(non_crossover_fraction))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() nonCrossoverFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(non_crossover_fraction) << " supplied)." << EidosTerminate();
	if ((gene_conversion_avg_length < 0.0) || std::isnan(gene_conversion_avg_length))		// intentionally no upper bound
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() meanLength must be >= 0.0 (" << EidosStringForFloat(gene_conversion_avg_length) << " supplied)." << EidosTerminate();
	if ((simple_conversion_fraction < 0.0) || (simple_conversion_fraction > 1.0) || std::isnan(simple_conversion_fraction))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() simpleConversionFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(simple_conversion_fraction) << " supplied)." << EidosTerminate();
	if ((bias < -1.0) || (bias > 1.0) || std::isnan(bias))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() bias must be between -1.0 and 1.0 inclusive (" << EidosStringForFloat(bias) << " supplied)." << EidosTerminate();
	if ((bias != 0.0) && !nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() bias must be 0.0 in non-nucleotide-based models." << EidosTerminate();
	
	chromosome_.using_DSB_model_ = true;
	chromosome_.non_crossover_fraction_ = non_crossover_fraction;
	chromosome_.gene_conversion_avg_length_ = gene_conversion_avg_length;
	chromosome_.gene_conversion_inv_half_length_ = 1.0 / (gene_conversion_avg_length / 2.0);
	chromosome_.simple_conversion_fraction_ = simple_conversion_fraction;
	chromosome_.mismatch_repair_bias_ = bias;
	
	if (SLiM_verbosity_level >= 1)
		output_stream << "initializeGeneConversion(" << non_crossover_fraction << ", " << gene_conversion_avg_length << ", " << simple_conversion_fraction << ", " << bias << ");" << std::endl;
	
	num_gene_conversions_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeHotspotMap(numeric multipliers, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeHotspotMap(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	if (!nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() may only be called in nucleotide-based models (use initializeMutationRate() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	EidosValue *multipliers_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	int multipliers_count = multipliers_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex;
	std::string sex_string = sex_value->StringAtIndex(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requested sex \"" << sex_string << "\" unsupported." << EidosTerminate();
	
	if ((requested_sex != IndividualSex::kUnspecified) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() sex-specific hotspot map supplied in non-sexual simulation." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && ((chromosome_.hotspot_multipliers_M_.size() != 0) || (chromosome_.hotspot_multipliers_F_.size() != 0))) ||
		((requested_sex != IndividualSex::kUnspecified) && (chromosome_.hotspot_multipliers_H_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	if (((requested_sex == IndividualSex::kUnspecified) && (num_hotspot_maps_ > 0)) || ((requested_sex != IndividualSex::kUnspecified) && (num_hotspot_maps_ > 1)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() may be called only once (or once per sex, with sex-specific hotspot maps).  The multiple hotspot regions of a hotspot map must be set up in a single call to initializeHotspotMap()." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? chromosome_.hotspot_end_positions_H_ : 
											   ((requested_sex == IndividualSex::kMale) ? chromosome_.hotspot_end_positions_M_ : chromosome_.hotspot_end_positions_F_));
	std::vector<double> &multipliers = ((requested_sex == IndividualSex::kUnspecified) ? chromosome_.hotspot_multipliers_H_ : 
								  ((requested_sex == IndividualSex::kMale) ? chromosome_.hotspot_multipliers_M_ : chromosome_.hotspot_multipliers_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		if (multipliers_count != 1)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires multipliers to be a singleton if ends is not supplied." << EidosTerminate();
		
		double multiplier = multipliers_value->FloatAtIndex(0, nullptr);
		
		// check values
		if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires multipliers to be >= 0 (" << EidosStringForFloat(multiplier) << " supplied)." << EidosTerminate();
		
		// then adopt them
		multipliers.clear();
		positions.clear();
		
		multipliers.emplace_back(multiplier);
		//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	else
	{
		int end_count = ends_value->Count();
		
		if ((end_count != multipliers_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires ends and multipliers to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double multiplier = multipliers_value->FloatAtIndex(value_index, nullptr);
			slim_position_t multiplier_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(value_index, nullptr));
			
			if (value_index > 0)
				if (multiplier_end_position <= ends_value->IntAtIndex(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires multipliers to be >= 0 (" << EidosStringForFloat(multiplier) << " supplied)." << EidosTerminate();
		}
		
		// then adopt them
		multipliers.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double multiplier = multipliers_value->FloatAtIndex(interval_index, nullptr);
			slim_position_t multiplier_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(interval_index, nullptr));
			
			multipliers.emplace_back(multiplier);
			positions.emplace_back(multiplier_end_position);
		}
	}
	
	chromosome_changed_ = true;
	
	if (SLiM_verbosity_level >= 1)
	{
		int multipliersSize = (int)multipliers.size();
		int endsSize = (int)positions.size();
		
		output_stream << "initializeHotspotMap(";
		
		if (multipliersSize > 1)
			output_stream << "c(";
		for (int interval_index = 0; interval_index < multipliersSize; ++interval_index)
		{
			if (interval_index >= 50)
			{
				output_stream << ", ...";
				break;
			}
			
			output_stream << (interval_index == 0 ? "" : ", ") << multipliers[interval_index];
		}
		if (multipliersSize > 1)
			output_stream << ")";
		
		if (endsSize > 0)
		{
			output_stream << ", ";
			
			if (endsSize > 1)
				output_stream << "c(";
			for (int interval_index = 0; interval_index < endsSize; ++interval_index)
			{
				if (interval_index >= 50)
				{
					output_stream << ", ...";
					break;
				}
				
				output_stream << (interval_index == 0 ? "" : ", ") << positions[interval_index];
			}
			if (endsSize > 1)
				output_stream << ")";
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_hotspot_maps_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeMutationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeMutationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	if (nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() may not be called in nucleotide-based models (use initializeHotspotMap() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex;
	std::string sex_string = sex_value->StringAtIndex(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requested sex \"" << sex_string << "\" unsupported." << EidosTerminate();
	
	if ((requested_sex != IndividualSex::kUnspecified) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() sex-specific mutation map supplied in non-sexual simulation." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state.  Since single_mutation_map_ has not been set
	// yet, we just look to see whether the chromosome's policy has already been determined or not.
	if (((requested_sex == IndividualSex::kUnspecified) && ((chromosome_.mutation_rates_M_.size() != 0) || (chromosome_.mutation_rates_F_.size() != 0))) ||
		((requested_sex != IndividualSex::kUnspecified) && (chromosome_.mutation_rates_H_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	if (((requested_sex == IndividualSex::kUnspecified) && (num_mutation_rates_ > 0)) || ((requested_sex != IndividualSex::kUnspecified) && (num_mutation_rates_ > 1)))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() may be called only once (or once per sex, with sex-specific mutation maps).  The multiple mutation regions of a mutation map must be set up in a single call to initializeMutationRate()." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? chromosome_.mutation_end_positions_H_ : 
											   ((requested_sex == IndividualSex::kMale) ? chromosome_.mutation_end_positions_M_ : chromosome_.mutation_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? chromosome_.mutation_rates_H_ : 
								  ((requested_sex == IndividualSex::kMale) ? chromosome_.mutation_rates_M_ : chromosome_.mutation_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double mutation_rate = rates_value->FloatAtIndex(0, nullptr);
		
		// check values
		if ((mutation_rate < 0.0) || (mutation_rate >= 1.0) || !std::isfinite(mutation_rate))		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires rates to be >= 0.0 and < 1.0 (" << EidosStringForFloat(mutation_rate) << " supplied)." << EidosTerminate();
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		rates.emplace_back(mutation_rate);
		//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	else
	{
		int end_count = ends_value->Count();
		
		if ((end_count != rate_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double mutation_rate = rates_value->FloatAtIndex(value_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(value_index, nullptr));
			
			if (value_index > 0)
				if (mutation_end_position <= ends_value->IntAtIndex(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((mutation_rate < 0.0) || (mutation_rate >= 1.0) || !std::isfinite(mutation_rate))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires rates to be >= 0.0 and < 1.0 (" << EidosStringForFloat(mutation_rate) << " supplied)." << EidosTerminate();
		}
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double mutation_rate = rates_value->FloatAtIndex(interval_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(interval_index, nullptr));
			
			rates.emplace_back(mutation_rate);
			positions.emplace_back(mutation_end_position);
		}
	}
	
	chromosome_changed_ = true;
	
	if (SLiM_verbosity_level >= 1)
	{
		int ratesSize = (int)rates.size();
		int endsSize = (int)positions.size();
		
		output_stream << "initializeMutationRate(";
		
		if (ratesSize > 1)
			output_stream << "c(";
		for (int interval_index = 0; interval_index < ratesSize; ++interval_index)
		{
			if (interval_index >= 50)
			{
				output_stream << ", ...";
				break;
			}
			
			output_stream << (interval_index == 0 ? "" : ", ") << rates[interval_index];
		}
		if (ratesSize > 1)
			output_stream << ")";
		
		if (endsSize > 0)
		{
			output_stream << ", ";
			
			if (endsSize > 1)
				output_stream << "c(";
			for (int interval_index = 0; interval_index < endsSize; ++interval_index)
			{
				if (interval_index >= 50)
				{
					output_stream << ", ...";
					break;
				}
				
				output_stream << (interval_index == 0 ? "" : ", ") << positions[interval_index];
			}
			if (endsSize > 1)
				output_stream << ")";
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_mutation_rates_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeSex(string$ chromosomeType, [numeric$ xDominanceCoeff = 1])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeSex(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *chromosomeType_value = p_arguments[0].get();
	EidosValue *xDominanceCoeff_value = p_arguments[1].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_sex_declarations_ > 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSex): initializeSex() may be called only once." << EidosTerminate();
	
	std::string chromosome_type = chromosomeType_value->StringAtIndex(0, nullptr);
	
	if (chromosome_type.compare(gStr_A) == 0)
		modeled_chromosome_type_ = GenomeType::kAutosome;
	else if (chromosome_type.compare(gStr_X) == 0)
		modeled_chromosome_type_ = GenomeType::kXChromosome;
	else if (chromosome_type.compare(gStr_Y) == 0)
		modeled_chromosome_type_ = GenomeType::kYChromosome;
	else
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSex): initializeSex() requires a chromosomeType of \"A\", \"X\", or \"Y\" (\"" << chromosome_type << "\" supplied)." << EidosTerminate();
	
	if (xDominanceCoeff_value->FloatAtIndex(0, nullptr) != 1.0)
	{
		if (modeled_chromosome_type_ == GenomeType::kXChromosome)
			x_chromosome_dominance_coeff_ = xDominanceCoeff_value->FloatAtIndex(0, nullptr);		// intentionally no bounds check
		else
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSex): initializeSex() xDominanceCoeff may be supplied only for chromosomeType \"X\"." << EidosTerminate();
	}
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeSex(\"" << chromosome_type << "\"";
		
		if (modeled_chromosome_type_ == GenomeType::kXChromosome)
			output_stream << ", " << x_chromosome_dominance_coeff_;
		
		output_stream << ");" << std::endl;
	}
	
	sex_enabled_ = true;
	num_sex_declarations_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeSLiMOptions([logical$ keepPedigrees = F], [string$ dimensionality = ""], [string$ periodicity = ""], [integer$ mutationRuns = 0], [logical$ preventIncidentalSelfing = F], [logical$ nucleotideBased = F])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeSLiMOptions(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_interpreter)
	//EidosValue *arg_keepPedigrees_value = p_arguments[0].get();
	EidosValue *arg_dimensionality_value = p_arguments[1].get();
	EidosValue *arg_periodicity_value = p_arguments[2].get();
	EidosValue *arg_mutationRuns_value = p_arguments[3].get();
	EidosValue *arg_preventIncidentalSelfing_value = p_arguments[4].get();
	EidosValue *arg_nucleotideBased_value = p_arguments[5].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_options_declarations_ > 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMOptions): initializeSLiMOptions() may be called only once." << EidosTerminate();
	
	if ((num_interaction_types_ > 0) || (num_mutation_types_ > 0) || (num_mutation_rates_ > 0) || (num_genomic_element_types_ > 0) || (num_genomic_elements_ > 0) || (num_recombination_rates_ > 0) || (num_gene_conversions_ > 0) || (num_sex_declarations_ > 0) || (num_treeseq_declarations_ > 0) || (num_ancseq_declarations_ > 0) || (num_hotspot_maps_ > 0))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMOptions): initializeSLiMOptions() must be called before all other initialization functions except initializeSLiMModelType()." << EidosTerminate();
	
	{
		// BCH 3 Sept. 2020: this flag is deprecated; pedigree tracking is now ALWAYS ENABLED
		// [logical$ keepPedigrees = F]
		//bool keep_pedigrees = arg_keepPedigrees_value->LogicalAtIndex(0, nullptr);
	}
	
	{
		// [string$ dimensionality = ""]
		std::string space = arg_dimensionality_value->StringAtIndex(0, nullptr);
		
		if (space.length() != 0)
		{
			if (space == "x")
				spatial_dimensionality_ = 1;
			else if (space == "xy")
				spatial_dimensionality_ = 2;
			else if (space == "xyz")
				spatial_dimensionality_ = 3;
			else
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), legal non-empty values for parameter dimensionality are only 'x', 'xy', and 'xyz'." << EidosTerminate();
		}
	}
	
	{
		// [string$ periodicity = ""]
		std::string periodicity = arg_periodicity_value->StringAtIndex(0, nullptr);
		
		if (periodicity.length() != 0)
		{
			if (spatial_dimensionality_ == 0)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), parameter periodicity may not be set in non-spatial simulations." << EidosTerminate();
			
			if (periodicity == "x")
				periodic_x_ = true;
			else if (periodicity == "y")
				periodic_y_ = true;
			else if (periodicity == "z")
				periodic_z_ = true;
			else if (periodicity == "xy")
				periodic_x_ = periodic_y_ = true;
			else if (periodicity == "xz")
				periodic_x_ = periodic_z_ = true;
			else if (periodicity == "yz")
				periodic_y_ = periodic_z_ = true;
			else if (periodicity == "xyz")
				periodic_x_ = periodic_y_ = periodic_z_ = true;
			else
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), legal non-empty values for parameter periodicity are only 'x', 'y', 'z', 'xy', 'xz', 'yz', and 'xyz'." << EidosTerminate();
			
			if ((periodic_y_ && (spatial_dimensionality_ < 2)) || (periodic_z_ && (spatial_dimensionality_ < 3)))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), parameter periodicity cannot utilize spatial dimensions beyond those set by the dimensionality parameter of initializeSLiMOptions()." << EidosTerminate();
		}
	}
	
	{
		// [integer$ mutationRuns = 0]
		int64_t mutrun_count = arg_mutationRuns_value->IntAtIndex(0, nullptr);
		
		if (mutrun_count != 0)
		{
			if ((mutrun_count < 1) || (mutrun_count > 10000))
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), parameter mutationRuns currently must be between 1 and 10000, inclusive." << EidosTerminate();
			
			preferred_mutrun_count_ = (int)mutrun_count;
		}
	}
	
	{
		// [logical$ preventIncidentalSelfing = F]
		bool prevent_selfing = arg_preventIncidentalSelfing_value->LogicalAtIndex(0, nullptr);
		
		prevent_incidental_selfing_ = prevent_selfing;
	}
	
	{
		// [logical$ nucleotideBased = F]
		bool nucleotide_based = arg_nucleotideBased_value->LogicalAtIndex(0, nullptr);
		
		nucleotide_based_ = nucleotide_based;
	}
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeSLiMOptions(";
		
		bool previous_params = false;
		
		if (spatial_dimensionality_ != 0)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "dimensionality = ";
			
			if (spatial_dimensionality_ == 1) output_stream << "'x'";
			else if (spatial_dimensionality_ == 2) output_stream << "'xy'";
			else if (spatial_dimensionality_ == 3) output_stream << "'xyz'";
			
			previous_params = true;
		}
		
		if (periodic_x_ || periodic_y_ || periodic_z_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "periodicity = '";
			
			if (periodic_x_) output_stream << "x";
			if (periodic_y_) output_stream << "y";
			if (periodic_z_) output_stream << "z";
			output_stream << "'";
			
			previous_params = true;
		}
		
		if (preferred_mutrun_count_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "mutationRunCount = " << preferred_mutrun_count_;
			previous_params = true;
		}
		
		if (prevent_incidental_selfing_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "preventIncidentalSelfing = " << (prevent_incidental_selfing_ ? "T" : "F");
			previous_params = true;
		}
		
		if (nucleotide_based_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "nucleotideBased = " << (nucleotide_based_ ? "T" : "F");
			previous_params = true;
			(void)previous_params;	// dead store above is deliberate
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_options_declarations_++;
	
	return gStaticEidosValueVOID;
}

// TREE SEQUENCE RECORDING
//	*********************	(void)initializeTreeSeq([logical$ recordMutations = T], [Nif$ simplificationRatio = NULL], [Ni$ simplificationInterval = NULL], [logical$ checkCoalescence = F], [logical$ runCrosschecks = F])
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeTreeSeq(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_interpreter)
	EidosValue *arg_recordMutations_value = p_arguments[0].get();
	EidosValue *arg_simplificationRatio_value = p_arguments[1].get();
	EidosValue *arg_simplificationInterval_value = p_arguments[2].get();
	EidosValue *arg_checkCoalescence_value = p_arguments[3].get();
	EidosValue *arg_runCrosschecks_value = p_arguments[4].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_treeseq_declarations_ > 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() may be called only once." << EidosTerminate();
	
	// NOTE: the TSXC_Enable() method also sets up tree-seq recording by setting these sorts of flags;
	// if the code here changes, that method should probably be updated too.
	
	recording_tree_ = true;
	recording_mutations_ = arg_recordMutations_value->LogicalAtIndex(0, nullptr);
	running_coalescence_checks_ = arg_checkCoalescence_value->LogicalAtIndex(0, nullptr);
	running_treeseq_crosschecks_ = arg_runCrosschecks_value->LogicalAtIndex(0, nullptr);
	treeseq_crosschecks_interval_ = 1;		// this interval is presently not exposed in the Eidos API
	
	if ((arg_simplificationRatio_value->Type() == EidosValueType::kValueNULL) && (arg_simplificationInterval_value->Type() == EidosValueType::kValueNULL))
	{
		// Both ratio and interval are NULL; use the default behavior of a ratio of 10
		simplification_ratio_ = 10.0;
		simplification_interval_ = -1;
		simplify_interval_ = 20;
	}
	else if (arg_simplificationRatio_value->Type() != EidosValueType::kValueNULL)
	{
		// The ratio is non-NULL; using the specified ratio
		simplification_ratio_ = arg_simplificationRatio_value->FloatAtIndex(0, nullptr);
		simplification_interval_ = -1;
		
		if (std::isnan(simplification_ratio_) || (simplification_ratio_ < 0))
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() requires simplificationRatio to be >= 0." << EidosTerminate();
		
		// Choose an initial auto-simplification interval
		if (arg_simplificationInterval_value->Type() != EidosValueType::kValueNULL)
		{
			// Both ratio and interval are non-NULL; the interval is thus interpreted as the *initial* interval
			simplify_interval_ = arg_simplificationInterval_value->IntAtIndex(0, nullptr);
			
			if (simplify_interval_ <= 0)
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() requires simplificationInterval to be > 0." << EidosTerminate();
		}
		else
		{
			// The interval is NULL, so use the default
			if (simplification_ratio_ == 0.0)
				simplify_interval_ = 1.0;
			else
				simplify_interval_ = 20;
		}
	}
	else if (arg_simplificationInterval_value->Type() != EidosValueType::kValueNULL)
	{
		// The ratio is NULL, interval is not; using the specified interval
		simplification_ratio_ = 0.0;
		simplification_interval_ = arg_simplificationInterval_value->IntAtIndex(0, nullptr);
		
		if (simplification_interval_ <= 0)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() requires simplificationInterval to be > 0." << EidosTerminate();
	}
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeTreeSeq(";
		
		bool previous_params = false;
		
		if (!recording_mutations_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "recordMutations = " << (recording_mutations_ ? "T" : "F");
			previous_params = true;
		}
		
		if (arg_simplificationRatio_value->Type() != EidosValueType::kValueNULL)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "simplificationRatio = " << simplification_ratio_;
			previous_params = true;
		}
		
		if (arg_simplificationInterval_value->Type() != EidosValueType::kValueNULL)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "simplificationInterval = " << arg_simplificationInterval_value->IntAtIndex(0, nullptr);
			previous_params = true;
		}
		
		if (running_coalescence_checks_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "checkCoalescence = " << (running_coalescence_checks_ ? "T" : "F");
			previous_params = true;
		}
		
		if (running_treeseq_crosschecks_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "runCrosschecks = " << (running_treeseq_crosschecks_ ? "T" : "F");
			previous_params = true;
			(void)previous_params;	// dead store above is deliberate
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_treeseq_declarations_++;
	
	return gStaticEidosValueVOID;
}


//	*********************	(void)initializeSLiMModelType(string$ modelType)
//
EidosValue_SP SLiMSim::ExecuteContextFunction_initializeSLiMModelType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_interpreter)
	EidosValue *arg_modelType_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_modeltype_declarations_ > 0)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMModelType): initializeSLiMModelType() may be called only once." << EidosTerminate();
	
	if ((num_interaction_types_ > 0) || (num_mutation_types_ > 0) || (num_mutation_rates_ > 0) || (num_genomic_element_types_ > 0) || (num_genomic_elements_ > 0) || (num_recombination_rates_ > 0) || (num_gene_conversions_ > 0) || (num_sex_declarations_ > 0) || (num_options_declarations_ > 0) || (num_treeseq_declarations_ > 0) || (num_ancseq_declarations_ > 0) || (num_hotspot_maps_ > 0))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMModelType): initializeSLiMModelType() must be called before all other initialization functions." << EidosTerminate();
	
	{
		// string$ modelType
		std::string model_type = arg_modelType_value->StringAtIndex(0, nullptr);
		
		if (model_type == "WF")
			model_type_ = SLiMModelType::kModelTypeWF;
		else if (model_type == "nonWF")
			model_type_ = SLiMModelType::kModelTypeNonWF;
		else
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteContextFunction_initializeSLiMModelType): in initializeSLiMModelType(), legal values for parameter modelType are only 'WF' or 'nonWF'." << EidosTerminate();
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

const std::vector<EidosFunctionSignature_CSP> *SLiMSim::ZeroGenerationFunctionSignatures(void)
{
	// Allocate our own EidosFunctionSignature objects
	static std::vector<EidosFunctionSignature_CSP> sim_0_signatures_;
	
	if (!sim_0_signatures_.size())
	{
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
										->AddNumeric_S("nonCrossoverFraction")->AddNumeric_S("meanLength")->AddNumeric_S("simpleConversionFraction")->AddNumeric_OS("bias", gStaticEidosValue_Integer0));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeMutationRate, nullptr, kEidosValueMaskVOID, "SLiM"))
										->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeHotspotMap, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddNumeric("multipliers")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSex, nullptr, kEidosValueMaskVOID, "SLiM"))
										->AddString_S("chromosomeType")->AddNumeric_OS("xDominanceCoeff", gStaticEidosValue_Float1));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSLiMOptions, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddLogical_OS("keepPedigrees", gStaticEidosValue_LogicalF)->AddString_OS("dimensionality", gStaticEidosValue_StringEmpty)->AddString_OS("periodicity", gStaticEidosValue_StringEmpty)->AddInt_OS("mutationRuns", gStaticEidosValue_Integer0)->AddLogical_OS("preventIncidentalSelfing", gStaticEidosValue_LogicalF)->AddLogical_OS("nucleotideBased", gStaticEidosValue_LogicalF));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeTreeSeq, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddLogical_OS("recordMutations", gStaticEidosValue_LogicalT)->AddNumeric_OSN("simplificationRatio", gStaticEidosValueNULL)->AddInt_OSN("simplificationInterval", gStaticEidosValueNULL)->AddLogical_OS("checkCoalescence", gStaticEidosValue_LogicalF)->AddLogical_OS("runCrosschecks", gStaticEidosValue_LogicalF));
		sim_0_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gStr_initializeSLiMModelType, nullptr, kEidosValueMaskVOID, "SLiM"))
									   ->AddString_S("modelType"));
	}
	
	return &sim_0_signatures_;
}

void SLiMSim::AddZeroGenerationFunctionsToMap(EidosFunctionMap &p_map)
{
	const std::vector<EidosFunctionSignature_CSP> *signatures = ZeroGenerationFunctionSignatures();
	
	if (signatures)
	{
		for (const EidosFunctionSignature_CSP &signature : *signatures)
			p_map.insert(EidosFunctionMapPair(signature->call_name_, signature));
	}
}

void SLiMSim::RemoveZeroGenerationFunctionsFromMap(EidosFunctionMap &p_map)
{
	const std::vector<EidosFunctionSignature_CSP> *signatures = ZeroGenerationFunctionSignatures();
	
	if (signatures)
	{
		for (const EidosFunctionSignature_CSP &signature : *signatures)
			p_map.erase(signature->call_name_);
	}
}

void SLiMSim::AddSLiMFunctionsToMap(EidosFunctionMap &p_map)
{
	const std::vector<EidosFunctionSignature_CSP> *signatures = SLiMFunctionSignatures();
	
	if (signatures)
	{
		for (const EidosFunctionSignature_CSP &signature : *signatures)
			p_map.insert(EidosFunctionMapPair(signature->call_name_, signature));
	}
}

const std::vector<EidosMethodSignature_CSP> *SLiMSim::AllMethodSignatures(void)
{
	static std::vector<EidosMethodSignature_CSP> *methodSignatures = nullptr;
	
	if (!methodSignatures)
	{
		const std::vector<EidosMethodSignature_CSP> *baseMethods =					gEidos_UndefinedClassObject->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsChromosome =			gSLiM_Chromosome_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsGenome =				gSLiM_Genome_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsGenomicElement =		gSLiM_GenomicElement_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsGenomicElementType =	gSLiM_GenomicElementType_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsIndividual =			gSLiM_Individual_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsInteractionType =		gSLiM_InteractionType_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsMutation =				gSLiM_Mutation_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsMutationType =			gSLiM_MutationType_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsSLiMEidosBlock =		gSLiM_SLiMEidosBlock_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsSLiMSim =				gSLiM_SLiMSim_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsSubpopulation =			gSLiM_Subpopulation_Class->Methods();
		const std::vector<EidosMethodSignature_CSP> *methodsSubstitution =			gSLiM_Substitution_Class->Methods();
		
		methodSignatures = new std::vector<EidosMethodSignature_CSP>(*baseMethods);
		
		methodSignatures->insert(methodSignatures->end(), methodsChromosome->begin(), methodsChromosome->end());
		methodSignatures->insert(methodSignatures->end(), methodsGenome->begin(), methodsGenome->end());
		methodSignatures->insert(methodSignatures->end(), methodsGenomicElement->begin(), methodsGenomicElement->end());
		methodSignatures->insert(methodSignatures->end(), methodsGenomicElementType->begin(), methodsGenomicElementType->end());
		methodSignatures->insert(methodSignatures->end(), methodsIndividual->begin(), methodsIndividual->end());
		methodSignatures->insert(methodSignatures->end(), methodsInteractionType->begin(), methodsInteractionType->end());
		methodSignatures->insert(methodSignatures->end(), methodsMutation->begin(), methodsMutation->end());
		methodSignatures->insert(methodSignatures->end(), methodsMutationType->begin(), methodsMutationType->end());
		methodSignatures->insert(methodSignatures->end(), methodsSLiMEidosBlock->begin(), methodsSLiMEidosBlock->end());
		methodSignatures->insert(methodSignatures->end(), methodsSLiMSim->begin(), methodsSLiMSim->end());
		methodSignatures->insert(methodSignatures->end(), methodsSubpopulation->begin(), methodsSubpopulation->end());
		methodSignatures->insert(methodSignatures->end(), methodsSubstitution->begin(), methodsSubstitution->end());
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(methodSignatures->begin(), methodSignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(methodSignatures->begin(), methodSignatures->end());
		methodSignatures->resize(std::distance(methodSignatures->begin(), unique_end_iter));
		
		// print out any signatures that are identical by name
		std::sort(methodSignatures->begin(), methodSignatures->end(), CompareEidosCallSignatures);
		
		EidosMethodSignature_CSP previous_sig = nullptr;
		
		for (const EidosMethodSignature_CSP &sig : *methodSignatures)
		{
			if (previous_sig && (sig->call_name_.compare(previous_sig->call_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the method signatures are identical.
				const EidosMethodSignature *sig1 = sig.get();
				const EidosMethodSignature *sig2 = previous_sig.get();
				
				if ((typeid(*sig1) != typeid(*sig2)) ||
					(sig->is_class_method != previous_sig->is_class_method) ||
					(sig->call_name_ != previous_sig->call_name_) ||
					(sig->return_mask_ != previous_sig->return_mask_) ||
					(sig->return_class_ != previous_sig->return_class_) ||
					(sig->arg_masks_ != previous_sig->arg_masks_) ||
					(sig->arg_names_ != previous_sig->arg_names_) ||
					(sig->arg_classes_ != previous_sig->arg_classes_) ||
					(sig->has_optional_args_ != previous_sig->has_optional_args_) ||
					(sig->has_ellipsis_ != previous_sig->has_ellipsis_))
					std::cout << "Duplicate method name with a different signature: " << sig->call_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return methodSignatures;
}

const std::vector<EidosPropertySignature_CSP> *SLiMSim::AllPropertySignatures(void)
{
	static std::vector<EidosPropertySignature_CSP> *propertySignatures = nullptr;
	
	if (!propertySignatures)
	{
		const std::vector<EidosPropertySignature_CSP> *baseProperties =					gEidos_UndefinedClassObject->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesChromosome =			gSLiM_Chromosome_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesGenome =				gSLiM_Genome_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesGenomicElement =		gSLiM_GenomicElement_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesGenomicElementType =	gSLiM_GenomicElementType_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesIndividual =			gSLiM_Individual_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesInteractionType =		gSLiM_InteractionType_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesMutation =				gSLiM_Mutation_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesMutationType =			gSLiM_MutationType_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesSLiMEidosBlock =		gSLiM_SLiMEidosBlock_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesSLiMSim =				gSLiM_SLiMSim_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesSubpopulation =		gSLiM_Subpopulation_Class->Properties();
		const std::vector<EidosPropertySignature_CSP> *propertiesSubstitution =			gSLiM_Substitution_Class->Properties();
		
		propertySignatures = new std::vector<EidosPropertySignature_CSP>(*baseProperties);
		
		propertySignatures->insert(propertySignatures->end(), propertiesChromosome->begin(), propertiesChromosome->end());
		propertySignatures->insert(propertySignatures->end(), propertiesGenome->begin(), propertiesGenome->end());
		propertySignatures->insert(propertySignatures->end(), propertiesGenomicElement->begin(), propertiesGenomicElement->end());
		propertySignatures->insert(propertySignatures->end(), propertiesGenomicElementType->begin(), propertiesGenomicElementType->end());
		propertySignatures->insert(propertySignatures->end(), propertiesIndividual->begin(), propertiesIndividual->end());
		propertySignatures->insert(propertySignatures->end(), propertiesInteractionType->begin(), propertiesInteractionType->end());
		propertySignatures->insert(propertySignatures->end(), propertiesMutation->begin(), propertiesMutation->end());
		propertySignatures->insert(propertySignatures->end(), propertiesMutationType->begin(), propertiesMutationType->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSLiMEidosBlock->begin(), propertiesSLiMEidosBlock->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSLiMSim->begin(), propertiesSLiMSim->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSubpopulation->begin(), propertiesSubpopulation->end());
		propertySignatures->insert(propertySignatures->end(), propertiesSubstitution->begin(), propertiesSubstitution->end());
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(propertySignatures->begin(), propertySignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(propertySignatures->begin(), propertySignatures->end());
		propertySignatures->resize(std::distance(propertySignatures->begin(), unique_end_iter));
		
		// print out any signatures that are identical by name
		std::sort(propertySignatures->begin(), propertySignatures->end(), CompareEidosPropertySignatures);
		
		EidosPropertySignature_CSP previous_sig = nullptr;
		
		for (const EidosPropertySignature_CSP &sig : *propertySignatures)
		{
			if (previous_sig && (sig->property_name_.compare(previous_sig->property_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the property signatures are identical.
				if ((sig->property_id_ != previous_sig->property_id_) ||
					(sig->read_only_ != previous_sig->read_only_) ||
					(sig->value_mask_ != previous_sig->value_mask_) ||
					(sig->value_class_ != previous_sig->value_class_))
					std::cout << "Duplicate property name with different signature: " << sig->property_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return propertySignatures;
}

EidosSymbolTable *SLiMSim::SymbolsFromBaseSymbols(EidosSymbolTable *p_base_symbols)
{
	// Since we keep our own symbol table long-term, this function does not actually re-derive a new table, but just returns the cached table
	if (p_base_symbols != gEidosConstantsSymbolTable)
		EIDOS_TERMINATION << "ERROR (SLiMSim::SymbolsFromBaseSymbols): (internal error) SLiM requires that its parent symbol table be the standard Eidos symbol table." << EidosTerminate();
	
	return simulation_constants_;
}

const EidosObjectClass *SLiMSim::Class(void) const
{
	return gSLiM_SLiMSim_Class;
}

EidosValue_SP SLiMSim::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_chromosome:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(&chromosome_, gSLiM_Chromosome_Class));
		case gID_chromosomeType:
		{
			switch (modeled_chromosome_type_)
			{
				case GenomeType::kAutosome:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_A));
				case GenomeType::kXChromosome:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_X));
				case GenomeType::kYChromosome:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_Y));
			}
			EIDOS_TERMINATION << "ERROR (SLiMSim::GetProperty): (internal error) unrecognized value for modeled_chromosome_type_." << EidosTerminate();
		}
		case gID_dimensionality:
		{
			static EidosValue_SP static_dimensionality_string_x;
			static EidosValue_SP static_dimensionality_string_xy;
			static EidosValue_SP static_dimensionality_string_xyz;
			
			if (!static_dimensionality_string_x)
			{
				static_dimensionality_string_x = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_x));
				static_dimensionality_string_xy = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xy"));
				static_dimensionality_string_xyz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xyz"));
			}
			
			switch (spatial_dimensionality_)
			{
				case 0:		return gStaticEidosValue_StringEmpty;
				case 1:		return static_dimensionality_string_x;
				case 2:		return static_dimensionality_string_xy;
				case 3:		return static_dimensionality_string_xyz;
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_periodicity:
		{
			static EidosValue_SP static_periodicity_string_x;
			static EidosValue_SP static_periodicity_string_y;
			static EidosValue_SP static_periodicity_string_z;
			static EidosValue_SP static_periodicity_string_xy;
			static EidosValue_SP static_periodicity_string_xz;
			static EidosValue_SP static_periodicity_string_yz;
			static EidosValue_SP static_periodicity_string_xyz;
			
			if (!static_periodicity_string_x)
			{
				static_periodicity_string_x = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_x));
				static_periodicity_string_y = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_y));
				static_periodicity_string_z = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_z));
				static_periodicity_string_xy = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xy"));
				static_periodicity_string_xz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xz"));
				static_periodicity_string_yz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("yz"));
				static_periodicity_string_xyz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xyz"));
			}
			
			if (periodic_x_ && periodic_y_ && periodic_z_)	return static_periodicity_string_xyz;
			else if (periodic_y_ && periodic_z_)			return static_periodicity_string_yz;
			else if (periodic_x_ && periodic_z_)			return static_periodicity_string_xz;
			else if (periodic_x_ && periodic_y_)			return static_periodicity_string_xy;
			else if (periodic_z_)							return static_periodicity_string_z;
			else if (periodic_y_)							return static_periodicity_string_y;
			else if (periodic_x_)							return static_periodicity_string_x;
			else											return gStaticEidosValue_StringEmpty;
		}
		case gID_genomicElementTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_GenomicElementType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto ge_type : genomic_element_types_)
				vec->push_object_element_NORR(ge_type.second);
			
			return result_SP;
		}
		case gID_inSLiMgui:
		{
			// THIS PROPERTY WAS DEPRECATED IN SLIM 3.2.1; use exists("slimgui") instead
			if (!warned_inSLiMgui_deprecated_)
			{
				if (!gEidosSuppressWarnings)
					SLIM_OUTSTREAM << "#WARNING (SLiMSim::GetProperty): the inSLiMgui property has been deprecated; use exists(\"slimgui\") instead." << std::endl;
				warned_inSLiMgui_deprecated_ = true;
			}
#ifdef SLIMGUI
			return gStaticEidosValue_LogicalT;
#else
			return gStaticEidosValue_LogicalF;
#endif
		}
		case gID_interactionTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_InteractionType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
            for (auto iter : interaction_types_)
				vec->push_object_element_NORR(iter.second);
			
			return result_SP;
		}
		case gID_modelType:
		{
			static EidosValue_SP static_model_type_string_WF;
			static EidosValue_SP static_model_type_string_nonWF;
			
			if (!static_model_type_string_WF)
			{
				static_model_type_string_WF = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("WF"));
				static_model_type_string_nonWF = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("nonWF"));
			}
			
			switch (model_type_)
			{
				case SLiMModelType::kModelTypeWF:		return static_model_type_string_WF;
				case SLiMModelType::kModelTypeNonWF:	return static_model_type_string_nonWF;
				default:								return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_mutations:
		{
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			int registry_size;
			const MutationIndex *registry = population_.MutationRegistry(&registry_size);
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->resize_no_initialize_RR(registry_size);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + registry[registry_index], registry_index);
			
			return result_SP;
		}
		case gID_mutationTypes:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_MutationType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto mutation_type : mutation_types_)
				vec->push_object_element_NORR(mutation_type.second);
			
			return result_SP;
		}
		case gID_nucleotideBased:
		{
			return (nucleotide_based_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_scriptBlocks:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_SLiMEidosBlock_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			std::vector<SLiMEidosBlock*> &script_blocks = AllScriptBlocks();
			
			for (auto script_block : script_blocks)
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
					vec->push_object_element_NORR(script_block);
			
			return result_SP;
		}
		case gID_sexEnabled:
			return (sex_enabled_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_subpopulations:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Subpopulation_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto pop : population_.subpops_)
				vec->push_object_element_NORR(pop.second);
			
			return result_SP;
		}
		case gID_substitutions:
		{
			std::vector<Substitution*> &substitutions = population_.substitutions_;
			int substitution_count = (int)substitutions.size();
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Substitution_Class))->resize_no_initialize_RR(substitution_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (int sub_index = 0; sub_index < substitution_count; ++sub_index)
				vec->set_object_element_no_check_no_previous_RR(substitutions[sub_index], sub_index);
			
			return result_SP;
		}
			
			// variables
		case gID_dominanceCoeffX:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x_chromosome_dominance_coeff_));
		case gID_generation:
		{
			if (!cached_value_generation_)
				cached_value_generation_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(generation_));
			return cached_value_generation_;
		}
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (SLiMSim::GetProperty): property tag accessed on simulation object before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void SLiMSim::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_generation:
		{
			int64_t value = p_value.IntAtIndex(0, nullptr);
			slim_generation_t old_generation = generation_;
			slim_generation_t new_generation = SLiMCastToGenerationTypeOrRaise(value);
			
			SetGeneration(new_generation);
			
			// Setting the generation into the future is generally harmless; the simulation logic is designed to handle that anyway, since
			// that happens every generation.  Setting the generation into the past is a bit tricker, since some things that have already
			// occurred need to be invalidated.  In particular, historical data cached by SLiMgui needs to be fixed.  Note that here we
			// do NOT remove Substitutions that are in the future, or otherwise try to backtrack the actual simulation state.  If the user
			// actually restores a past state with readFromPopulationFile(), all that kind of stuff will be reset; but if all they do is
			// set the generation counter back, the model state is unchanged, substitutions are still fixed, etc.  This means that the
			// simulation code needs to be robust to the possibility that some records, e.g. for Substitutions, may appear to be about
			// events in the future.  But usually users will only set the generation back if they also call readFromPopulationFile().
			if (generation_ < old_generation)
			{
#ifdef SLIMGUI
				// Fix fitness histories for SLiMgui.  Note that mutation_loss_times_ and mutation_fixation_times_ are not fixable, since
				// their entries are not separated out by generation, so we just leave them as is, containing information about
				// alternative futures of the model.
				for (auto history_record_iter : population_.fitness_histories_)
				{
					FitnessHistory &history_record = history_record_iter.second;
					double *history = history_record.history_;
					
					if (history)
					{
						int old_last_valid_history_index = std::max(0, old_generation - 2);		// if gen==2, gen 1 was the last valid entry, and it is at index 0
						int new_last_valid_history_index = std::max(0, generation_ - 2);		// ditto
						
						// make sure that we don't overrun the end of the buffer
						if (old_last_valid_history_index > history_record.history_length_ - 1)
							old_last_valid_history_index = history_record.history_length_ - 1;
						
						for (int entry_index = new_last_valid_history_index + 1; entry_index <= old_last_valid_history_index; ++entry_index)
							history[entry_index] = NAN;
					}
				}
                
                for (auto history_record_iter : population_.subpop_size_histories_)
				{
					SubpopSizeHistory &history_record = history_record_iter.second;
					slim_popsize_t *history = history_record.history_;
					
					if (history)
					{
						int old_last_valid_history_index = std::max(0, old_generation - 2);		// if gen==2, gen 1 was the last valid entry, and it is at index 0
						int new_last_valid_history_index = std::max(0, generation_ - 2);		// ditto
						
						// make sure that we don't overrun the end of the buffer
						if (old_last_valid_history_index > history_record.history_length_ - 1)
							old_last_valid_history_index = history_record.history_length_ - 1;
						
						for (int entry_index = new_last_valid_history_index + 1; entry_index <= old_last_valid_history_index; ++entry_index)
							history[entry_index] = 0;
					}
				}
#endif
			}
			
			return;
		}
			
		case gID_dominanceCoeffX:
		{
			if (!sex_enabled_ || (modeled_chromosome_type_ != GenomeType::kXChromosome))
				EIDOS_TERMINATION << "ERROR (SLiMSim::SetProperty): attempt to set property dominanceCoeffX when not simulating an X chromosome." << EidosTerminate();
			
			double value = p_value.FloatAtIndex(0, nullptr);
			
			x_chromosome_dominance_coeff_ = value;		// intentionally no bounds check
			return;
		}
			
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP SLiMSim::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
#ifdef SLIM_WF_ONLY
		case gID_addSubpopSplit:				return ExecuteMethod_addSubpopSplit(p_method_id, p_arguments, p_interpreter);
#endif	// SLIM_WF_ONLY
			
		case gID_addSubpop:						return ExecuteMethod_addSubpop(p_method_id, p_arguments, p_interpreter);
		case gID_deregisterScriptBlock:			return ExecuteMethod_deregisterScriptBlock(p_method_id, p_arguments, p_interpreter);
		case gID_mutationFrequencies:
		case gID_mutationCounts:				return ExecuteMethod_mutationFreqsCounts(p_method_id, p_arguments, p_interpreter);
		case gID_mutationsOfType:				return ExecuteMethod_mutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_countOfMutationsOfType:		return ExecuteMethod_countOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_outputFixedMutations:			return ExecuteMethod_outputFixedMutations(p_method_id, p_arguments, p_interpreter);
		case gID_outputFull:					return ExecuteMethod_outputFull(p_method_id, p_arguments, p_interpreter);
		case gID_outputMutations:				return ExecuteMethod_outputMutations(p_method_id, p_arguments, p_interpreter);
		case gID_outputUsage:					return ExecuteMethod_outputUsage(p_method_id, p_arguments, p_interpreter);
		case gID_readFromPopulationFile:		return ExecuteMethod_readFromPopulationFile(p_method_id, p_arguments, p_interpreter);
		case gID_recalculateFitness:			return ExecuteMethod_recalculateFitness(p_method_id, p_arguments, p_interpreter);
		case gID_registerEarlyEvent:
		case gID_registerLateEvent:				return ExecuteMethod_registerEarlyLateEvent(p_method_id, p_arguments, p_interpreter);
		case gID_registerFitnessCallback:		return ExecuteMethod_registerFitnessCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerInteractionCallback:	return ExecuteMethod_registerInteractionCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerMateChoiceCallback:
		case gID_registerModifyChildCallback:
		case gID_registerRecombinationCallback:	return ExecuteMethod_registerMateModifyRecCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerMutationCallback:		return ExecuteMethod_registerMutationCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerReproductionCallback:	return ExecuteMethod_registerReproductionCallback(p_method_id, p_arguments, p_interpreter);
		case gID_rescheduleScriptBlock:			return ExecuteMethod_rescheduleScriptBlock(p_method_id, p_arguments, p_interpreter);
		case gID_simulationFinished:			return ExecuteMethod_simulationFinished(p_method_id, p_arguments, p_interpreter);
		case gID_subsetMutations:				return ExecuteMethod_subsetMutations(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqCoalesced:				return ExecuteMethod_treeSeqCoalesced(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqSimplify:				return ExecuteMethod_treeSeqSimplify(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqRememberIndividuals:	return ExecuteMethod_treeSeqRememberIndividuals(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqOutput:					return ExecuteMethod_treeSeqOutput(p_method_id, p_arguments, p_interpreter);
		default:								return EidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (object<Subpopulation>$)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio = 0.5])
//
EidosValue_SP SLiMSim::ExecuteMethod_addSubpop(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMGenerationStage gen_stage = GenerationStage();
	
	if ((gen_stage != SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kWFStage5ExecuteLateScripts) &&
		(gen_stage != SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpop): addSubpop() may only be called from an early() or late() event." << EidosTerminate();
	if ((executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpop): addSubpop() may not be called from inside a callback." << EidosTerminate();
	
	EidosValue *subpopID_value = p_arguments[0].get();
	EidosValue *size_value = p_arguments[1].get();
	EidosValue *sexRatio_value = p_arguments[2].get();
	
	slim_objectid_t subpop_id = SLiM_ExtractObjectIDFromEidosValue_is(subpopID_value, 0, 'p');
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(size_value->IntAtIndex(0, nullptr));
	
	double sex_ratio = sexRatio_value->FloatAtIndex(0, nullptr);
	
	if ((sex_ratio != 0.5) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpop): addSubpop() sex ratio supplied in non-sexual simulation." << EidosTerminate();
	
	// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
	Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, sex_ratio);
	
	// define a new Eidos variable to refer to the new subpopulation
	EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpop): addSubpop() symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	
	return symbol_entry.second;
}

#ifdef SLIM_WF_ONLY
//	*********************	– (object<Subpopulation>$)addSubpopSplit(is$ subpopID, integer$ size, io<Subpopulation>$ sourceSubpop, [float$ sexRatio = 0.5])
//
EidosValue_SP SLiMSim::ExecuteMethod_addSubpopSplit(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (ModelType() == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpopSplit): method -addSubpopSplit() is not available in nonWF models." << EidosTerminate();
	
	SLiMGenerationStage gen_stage = GenerationStage();
	
	if ((gen_stage != SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kWFStage5ExecuteLateScripts) &&
		(gen_stage != SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpopSplit): addSubpopSplit() may only be called from an early() or late() event." << EidosTerminate();
	if ((executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpopSplit): addSubpopSplit() may not be called from inside a callback." << EidosTerminate();
	
	EidosValue *subpopID_value = p_arguments[0].get();
	EidosValue *size_value = p_arguments[1].get();
	EidosValue *sourceSubpop_value = p_arguments[2].get();
	EidosValue *sexRatio_value = p_arguments[3].get();
	
	slim_objectid_t subpop_id = SLiM_ExtractObjectIDFromEidosValue_is(subpopID_value, 0, 'p');
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(size_value->IntAtIndex(0, nullptr));
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	Subpopulation *source_subpop = SLiM_ExtractSubpopulationFromEidosValue_io(sourceSubpop_value, 0, sim, "addSubpopSplit()");
	
	double sex_ratio = sexRatio_value->FloatAtIndex(0, nullptr);
	
	if ((sex_ratio != 0.5) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpopSplit): addSubpopSplit() sex ratio supplied in non-sexual simulation." << EidosTerminate();
	
	// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
	Subpopulation *new_subpop = population_.AddSubpopulationSplit(subpop_id, *source_subpop, subpop_size, sex_ratio);
	
	// define a new Eidos variable to refer to the new subpopulation
	EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_addSubpopSplit): addSubpopSplit() symbol " << Eidos_StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	simulation_constants_->InitializeConstantSymbolEntry(symbol_entry);
	
	return symbol_entry.second;
}
#endif	// SLIM_WF_ONLY

//	*********************	- (void)deregisterScriptBlock(io<SLiMEidosBlock> scriptBlocks)
//
EidosValue_SP SLiMSim::ExecuteMethod_deregisterScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *scriptBlocks_value = p_arguments[0].get();
	
	int block_count = scriptBlocks_value->Count();
	
	// We just schedule the blocks for deregistration; we do not deregister them immediately, because that would leave stale pointers lying around
	for (int block_index = 0; block_index < block_count; ++block_index)
	{
		SLiMEidosBlock *block = SLiM_ExtractSLiMEidosBlockFromEidosValue_io(scriptBlocks_value, block_index, *this, "deregisterScriptBlock()");
		
		if (block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
		{
			// this should never be hit, because the user should have no way to get a reference to a function block
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_deregisterScriptBlock): (internal error) deregisterScriptBlock() cannot be called on user-defined function script blocks." << EidosTerminate();
		}
		else if (block->type_ == SLiMEidosBlockType::SLiMEidosInteractionCallback)
		{
			// interaction() callbacks have to work differently, because they can be called at any time after an
			// interaction has been evaluated, up until the interaction is invalidated; we can't make pointers
			// to interaction() callbacks go stale except at that specific point in the generation cycle
			if (std::find(scheduled_interaction_deregs_.begin(), scheduled_interaction_deregs_.end(), block) != scheduled_interaction_deregs_.end())
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_deregisterScriptBlock): deregisterScriptBlock() called twice on the same script block." << EidosTerminate();
			
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
			// all other script blocks go on the main list and get cleared out at the end of each generation stage
			if (std::find(scheduled_deregistrations_.begin(), scheduled_deregistrations_.end(), block) != scheduled_deregistrations_.end())
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_deregisterScriptBlock): deregisterScriptBlock() called twice on the same script block." << EidosTerminate();
			
			scheduled_deregistrations_.emplace_back(block);
			
#if DEBUG_BLOCK_REG_DEREG
			std::cout << "deregisterScriptBlock() called for block:" << std::endl;
			std::cout << "   ";
			block->Print(std::cout);
			std::cout << std::endl;
#endif
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	– (float)mutationFrequencies(No<Subpopulation> subpops, [No<Mutation> mutations = NULL])
//	*********************	– (integer)mutationCounts(No<Subpopulation> subpops, [No<Mutation> mutations = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_mutationFreqsCounts(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *subpops_value = p_arguments[0].get();
	EidosValue *mutations_value = p_arguments[1].get();
	
	slim_refcount_t total_genome_count = 0;
	
	// tally across the requested subpops
	if (subpops_value->Type() == EidosValueType::kValueNULL)
	{
		// tally across the whole population
		total_genome_count = population_.TallyMutationReferences(nullptr, false);
	}
	else
	{
		// requested subpops, so get them
		int requested_subpop_count = subpops_value->Count();
		static std::vector<Subpopulation*> subpops_to_tally;	// using and clearing a static prevents allocation thrash; should be safe from re-entry since TallyMutationReferences() can't re-enter here
		
		if (requested_subpop_count)
		{
			for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
				subpops_to_tally.emplace_back((Subpopulation *)(subpops_value->ObjectElementAtIndex(requested_subpop_index, nullptr)));
		}
		
		total_genome_count = population_.TallyMutationReferences(&subpops_to_tally, false);
		subpops_to_tally.clear();
	}
	
	// OK, now construct our result vector from the tallies for just the requested mutations
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	double denominator = 1.0 / total_genome_count;
	EidosValue_SP result_SP;
	
	// BCH 10/3/2020: Note that we now have to worry about being asked for the frequency of mutations that are
	// not in the registry, and might be fixed or lost.  We handle this in the first major case below, where
	// a vector of mutations was given.  It could be a marginal issue in the second major case, where NULL was
	// passed for the mutation vector, because the registry can temporarily contain mutations in the state
	// MutationState::kRemovedWithSubstitution, immediately after removeMutations(substitute=T); if that is
	// a potential issue, population_.registry_needs_consistency_check_ will be true, and we treat it specially.
	
	if (mutations_value->Type() != EidosValueType::kValueNULL)
	{
		// a vector of mutations was given, so loop through them and take their tallies
		int mutations_count = mutations_value->Count();
		
		if (mutations_count == 1)
		{
			// Handle the one-mutation case separately so we can return a singleton
			if (p_method_id == gID_mutationFrequencies)
			{
				Mutation *mut = (Mutation *)(mutations_value->ObjectElementAtIndex(0, nullptr));
				int8_t mut_state = mut->state_;
				double freq;
				
				if (mut_state == MutationState::kInRegistry)			freq = *(refcount_block_ptr + mut->BlockIndex()) * denominator;
				else if (mut_state == MutationState::kLostAndRemoved)	freq = 0.0;
				else													freq = 1.0;
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(freq));
			}
			else // p_method_id == gID_mutationCounts
			{
				Mutation *mut = (Mutation *)(mutations_value->ObjectElementAtIndex(0, nullptr));
				int8_t mut_state = mut->state_;
				slim_refcount_t count;
				
				if (mut_state == MutationState::kInRegistry)			count = *(refcount_block_ptr + mut->BlockIndex());
				else if (mut_state == MutationState::kLostAndRemoved)	count = 0;
				else													count = total_genome_count;
				
				result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(count));
			}
		}
		else
		{
			if (p_method_id == gID_mutationFrequencies)
			{
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(mutations_count);
				result_SP = EidosValue_SP(float_result);
				
				for (int value_index = 0; value_index < mutations_count; ++value_index)
				{
					Mutation *mut = (Mutation *)(mutations_value->ObjectElementAtIndex(value_index, nullptr));
					int8_t mut_state = mut->state_;
					double freq;
					
					if (mut_state == MutationState::kInRegistry)			freq = *(refcount_block_ptr + mut->BlockIndex()) * denominator;
					else if (mut_state == MutationState::kLostAndRemoved)	freq = 0.0;
					else													freq = 1.0;
					
					float_result->set_float_no_check(freq, value_index);
				}
			}
			else // p_method_id == gID_mutationCounts
			{
				EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(mutations_count);
				result_SP = EidosValue_SP(int_result);
				
				for (int value_index = 0; value_index < mutations_count; ++value_index)
				{
					Mutation *mut = (Mutation *)(mutations_value->ObjectElementAtIndex(value_index, nullptr));
					int8_t mut_state = mut->state_;
					slim_refcount_t count;
					
					if (mut_state == MutationState::kInRegistry)			count = *(refcount_block_ptr + mut->BlockIndex());
					else if (mut_state == MutationState::kLostAndRemoved)	count = 0;
					else													count = total_genome_count;
					
					int_result->set_int_no_check(count, value_index);
				}
			}
		}
	}
	else if (population_.MutationRegistryNeedsCheck())
	{
		// no mutation vector was given, so return all frequencies from the registry
		// this is the same as the case below, except MutationState::kRemovedWithSubstitution is possible
		int registry_size;
		const MutationIndex *registry = population_.MutationRegistry(&registry_size);
		Mutation *mutation_block_ptr = gSLiM_Mutation_Block;
		
		if (p_method_id == gID_mutationFrequencies)
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(registry_size);
			result_SP = EidosValue_SP(float_result);
			
			for (int registry_index = 0; registry_index < registry_size; registry_index++)
			{
				MutationIndex mut_index = registry[registry_index];
				int8_t mut_state = (mutation_block_ptr + registry_index)->state_;
				double freq;
				
				if (mut_state == MutationState::kInRegistry)		freq = *(refcount_block_ptr + mut_index) * denominator;
				else /* MutationState::kRemovedWithSubstitution */	freq = 1.0;
				
				float_result->set_float_no_check(freq, registry_index);
			}
		}
		else // p_method_id == gID_mutationCounts
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(registry_size);
			result_SP = EidosValue_SP(int_result);
			
			for (int registry_index = 0; registry_index < registry_size; registry_index++)
			{
				MutationIndex mut_index = registry[registry_index];
				int8_t mut_state = (mutation_block_ptr + registry_index)->state_;
				slim_refcount_t count;
				
				if (mut_state == MutationState::kInRegistry)		count = *(refcount_block_ptr + mut_index);
				else /* MutationState::kRemovedWithSubstitution */	count = total_genome_count;
				
				int_result->set_int_no_check(count, registry_index);
			}
		}
	}
	else
	{
		// no mutation vector was given, so return all frequencies from the registry
		int registry_size;
		const MutationIndex *registry = population_.MutationRegistry(&registry_size);
		
		if (p_method_id == gID_mutationFrequencies)
		{
			EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(registry_size);
			result_SP = EidosValue_SP(float_result);
			
			for (int registry_index = 0; registry_index < registry_size; registry_index++)
				float_result->set_float_no_check(*(refcount_block_ptr + registry[registry_index]) * denominator, registry_index);
		}
		else // p_method_id == gID_mutationCounts
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(registry_size);
			result_SP = EidosValue_SP(int_result);
			
			for (int registry_index = 0; registry_index < registry_size; registry_index++)
				int_result->set_int_no_check(*(refcount_block_ptr + registry[registry_index]), registry_index);
		}
	}
	
	return result_SP;
}

//	*********************	- (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP SLiMSim::ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, *this, "mutationsOfType()");
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	// track calls per generation to SLiMSim::ExecuteMethod_mutationsOfType() and SLiMSim::ExecuteMethod_countOfMutationsOfType()
	bool start_registry = (mutation_type_ptr->muttype_registry_call_count_++ >= 1);
	population_.any_muttype_call_count_used_ = true;
	
	// start a registry if appropriate, so we can hit the fast case below
	if (start_registry && (!population_.keeping_muttype_registries_ || !mutation_type_ptr->keeping_muttype_registry_))
	{
		int registry_size;
		const MutationIndex *registry = population_.MutationRegistry(&registry_size);
		MutationRun &muttype_registry = mutation_type_ptr->muttype_registry_;
		
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			MutationIndex mut = registry[registry_index];
			
			if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
				muttype_registry.emplace_back(mut);
		}
		
		population_.keeping_muttype_registries_ = true;
		mutation_type_ptr->keeping_muttype_registry_ = true;
	}
	
	if (population_.keeping_muttype_registries_ && mutation_type_ptr->keeping_muttype_registry_)
	{
		// We're already keeping a separate registry for this mutation type (see mutation_type.h), so we can answer this directly
		MutationRun &mutation_registry = mutation_type_ptr->muttype_registry_;
		int mutation_count = mutation_registry.size();
		
		if (mutation_count == 1)
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(mut_block_ptr + mutation_registry[0], gSLiM_Mutation_Class));
		}
		else
		{
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->resize_no_initialize_RR(mutation_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
				vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + mutation_registry[mut_index], mut_index);
			
			return result_SP;
		}
	}
	else
#endif
	{
		// No registry in the muttype; count the number of mutations of the given type, so we can reserve the right vector size
		// To avoid having to scan the registry twice for the simplest case of a single mutation, we cache the first mutation found
		int registry_size;
		const MutationIndex *registry = population_.MutationRegistry(&registry_size);
		int match_count = 0, registry_index;
		MutationIndex first_match = -1;
		
		for (registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			MutationIndex mut = registry[registry_index];
			
			if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
			{
				if (++match_count == 1)
					first_match = mut;
			}
		}
		
		// Now allocate the result vector and assemble it
		if (match_count == 1)
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(mut_block_ptr + first_match, gSLiM_Mutation_Class));
		}
		else
		{
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->resize_no_initialize_RR(match_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			if (match_count != 0)
			{
				int set_index = 0;
				
				for (registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					MutationIndex mut = registry[registry_index];
					
					if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
						vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + mut, set_index++);
				}
			}
			
			return result_SP;
		}
	}
}
			
//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP SLiMSim::ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, *this, "countOfMutationsOfType()");
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	// track calls per generation to SLiMSim::ExecuteMethod_mutationsOfType() and SLiMSim::ExecuteMethod_countOfMutationsOfType()
	bool start_registry = (mutation_type_ptr->muttype_registry_call_count_++ >= 1);
	population_.any_muttype_call_count_used_ = true;
	
	// start a registry if appropriate, so we can hit the fast case below
	if (start_registry && (!population_.keeping_muttype_registries_ || !mutation_type_ptr->keeping_muttype_registry_))
	{
		int registry_size;
		const MutationIndex *registry = population_.MutationRegistry(&registry_size);
		MutationRun &muttype_registry = mutation_type_ptr->muttype_registry_;
		
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			MutationIndex mut = registry[registry_index];
			
			if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
				muttype_registry.emplace_back(mut);
		}
		
		population_.keeping_muttype_registries_ = true;
		mutation_type_ptr->keeping_muttype_registry_ = true;
	}
	
	if (population_.keeping_muttype_registries_ && mutation_type_ptr->keeping_muttype_registry_)
	{
		// We're already keeping a separate registry for this mutation type (see mutation_type.h), so we can answer this directly
		MutationRun &muttype_registry = mutation_type_ptr->muttype_registry_;
		int mutation_count = muttype_registry.size();
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(mutation_count));
	}
	else
#endif
	{
		// Count the number of mutations of the given type
		int registry_size;
		const MutationIndex *registry = population_.MutationRegistry(&registry_size);
		int match_count = 0;
		
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
			if ((mut_block_ptr + registry[registry_index])->mutation_type_ptr_ == mutation_type_ptr)
				++match_count;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(match_count));
	}
}
			
//	*********************	– (void)outputFixedMutations([Ns$ filePath = NULL], [logical$ append=F])
//
EidosValue_SP SLiMSim::ExecuteMethod_outputFixedMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *append_value = p_arguments[1].get();
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (!warned_early_output_)
	{
		if (GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				output_stream << "#WARNING (SLiMSim::ExecuteMethod_outputFixedMutations): outputFixedMutations() should probably not be called from an early() event in a WF model; the output will reflect state at the beginning of the generation, not the end." << std::endl;
				warned_early_output_ = true;
			}
		}
	}
	
	std::ofstream outfile;
	bool has_file = false;
	std::string outfile_path;
	
	if (filePath_value->Type() != EidosValueType::kValueNULL)
	{
		outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex(0, nullptr));
		bool append = append_value->LogicalAtIndex(0, nullptr);
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		has_file = true;
		
		if (!outfile.is_open())
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_outputFixedMutations): outputFixedMutations() could not open "<< outfile_path << "." << EidosTerminate();
	}
	
	std::ostream &out = *(has_file ? dynamic_cast<std::ostream *>(&outfile) : dynamic_cast<std::ostream *>(&output_stream));
	
#if DO_MEMORY_CHECKS
	// This method can burn a huge amount of memory and get us killed, if we have a maximum memory usage.  It's nice to
	// try to check for that and terminate with a proper error message, to help the user diagnose the problem.
	int mem_check_counter = 0, mem_check_mod = 100;
	
	if (eidos_do_memory_checks)
		Eidos_CheckRSSAgainstMax("SLiMSim::ExecuteMethod_outputFixedMutations", "(outputFixedMutations(): The memory usage was already out of bounds on entry.)");
#endif
	
	// Output header line
	out << "#OUT: " << generation_ << " F";
	
	if (has_file)
		out << " " << outfile_path;
	
	out << std::endl;
	
	// Output Mutations section
	out << "Mutations:" << std::endl;
	
	std::vector<Substitution*> &subs = population_.substitutions_;
	
	for (unsigned int i = 0; i < subs.size(); i++)
	{
		out << i << " ";
		subs[i]->PrintForSLiMOutput(out);
		
#if DO_MEMORY_CHECKS
		if (eidos_do_memory_checks)
		{
			mem_check_counter++;
			
			if (mem_check_counter % mem_check_mod == 0)
				Eidos_CheckRSSAgainstMax("SLiMSim::ExecuteMethod_outputFixedMutations", "(outputFixedMutations(): Out of memory while outputting substitution objects.)");
		}
#endif
	}
	
	if (has_file)
		outfile.close(); 
	
	return gStaticEidosValueVOID;
}
			
//	*********************	– (void)outputFull([Ns$ filePath = NULL], [logical$ binary = F], [logical$ append=F], [logical$ spatialPositions = T], [logical$ ages = T], [logical$ ancestralNucleotides = T], [logical$ pedigreeIDs = F])
//
EidosValue_SP SLiMSim::ExecuteMethod_outputFull(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *binary_value = p_arguments[1].get();
	EidosValue *append_value = p_arguments[2].get();
	EidosValue *spatialPositions_value = p_arguments[3].get();
	EidosValue *ages_value = p_arguments[4].get();
	EidosValue *ancestralNucleotides_value = p_arguments[5].get();
	EidosValue *pedigreeIDs_value = p_arguments[6].get();
	
	if (!warned_early_output_)
	{
		if (GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (SLiMSim::ExecuteMethod_outputFull): outputFull() should probably not be called from an early() event in a WF model; the output will reflect state at the beginning of the generation, not the end." << std::endl;
				warned_early_output_ = true;
			}
		}
	}
	
	bool use_binary = binary_value->LogicalAtIndex(0, nullptr);
	bool output_spatial_positions = spatialPositions_value->LogicalAtIndex(0, nullptr);
	bool output_ages = ages_value->LogicalAtIndex(0, nullptr);
	bool output_ancestral_nucs = ancestralNucleotides_value->LogicalAtIndex(0, nullptr);
	bool output_pedigree_ids = pedigreeIDs_value->LogicalAtIndex(0, nullptr);
	
	if (filePath_value->Type() == EidosValueType::kValueNULL)
	{
		if (use_binary)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_outputFull): outputFull() cannot output in binary format to the standard output stream; specify a file for output." << EidosTerminate();
		
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		output_stream << "#OUT: " << generation_ << " A" << std::endl;
		population_.PrintAll(output_stream, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids);
	}
	else
	{
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex(0, nullptr));
		bool append = append_value->LogicalAtIndex(0, nullptr);
		std::ofstream outfile;
		
		if (use_binary && append)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_outputFull): outputFull() cannot append in binary format." << EidosTerminate();
		
		if (use_binary)
			outfile.open(outfile_path.c_str(), std::ios::out | std::ios::binary);
		else
			outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (outfile.is_open())
		{
			if (use_binary)
			{
				population_.PrintAllBinary(outfile, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids);
			}
			else
			{
				// We no longer have input parameters to print; possibly this should print all the initialize...() functions called...
				//				const std::vector<std::string> &input_parameters = p_sim.InputParameters();
				//				
				//				for (int i = 0; i < input_parameters.size(); i++)
				//					outfile << input_parameters[i] << endl;
				
				outfile << "#OUT: " << generation_ << " A " << outfile_path << std::endl;
				population_.PrintAll(outfile, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids);
			}
			
			outfile.close(); 
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_outputFull): outputFull() could not open "<< outfile_path << "." << EidosTerminate();
		}
	}
	
	return gStaticEidosValueVOID;
}
			
//	*********************	– (void)outputMutations(object<Mutation> mutations, [Ns$ filePath = NULL], [logical$ append=F])
//
EidosValue_SP SLiMSim::ExecuteMethod_outputMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	EidosValue *filePath_value = p_arguments[1].get();
	EidosValue *append_value = p_arguments[2].get();
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (!warned_early_output_)
	{
		if (GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				output_stream << "#WARNING (SLiMSim::ExecuteMethod_outputMutations): outputMutations() should probably not be called from an early() event in a WF model; the output will reflect state at the beginning of the generation, not the end." << std::endl;
				warned_early_output_ = true;
			}
		}
	}
	
	std::ofstream outfile;
	bool has_file = false;
	
	if (filePath_value->Type() != EidosValueType::kValueNULL)
	{
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex(0, nullptr));
		bool append = append_value->LogicalAtIndex(0, nullptr);
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		has_file = true;
		
		if (!outfile.is_open())
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_outputMutations): outputMutations() could not open "<< outfile_path << "." << EidosTerminate();
	}
	
	std::ostream &out = *(has_file ? (std::ostream *)&outfile : (std::ostream *)&output_stream);
	
	int mutations_count = mutations_value->Count();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if (mutations_count > 0)
	{
		// as we scan through genomes building the polymorphism map, we want to process only mutations that are
		// in the user-supplied mutations vector; to do that filtering efficiently, we use Mutation::scratch_
		// first zero out scratch_ in all mutations in the registry...
		int registry_size;
		const MutationIndex *registry = population_.MutationRegistry(&registry_size);
		
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			Mutation *mut = mut_block_ptr + registry[registry_index];
			mut->scratch_ = 0;
		}
		
		// ...then set scratch_ = 1 for all mutations that have been requested for output
		EidosValue_Object *mutations_object = (EidosValue_Object *)mutations_value;
		
		for (int mut_index = 0; mut_index < mutations_count; mut_index++)
		{
			Mutation *mut = (Mutation *)(mutations_object->ObjectElementAtIndex(mut_index, nullptr));
			mut->scratch_ = 1;
		}
		
		// find all polymorphisms of the mutations that are to be tracked
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			PolymorphismMap polymorphisms;
			
			for (slim_popsize_t i = 0; i < 2 * subpop->parent_subpop_size_; i++)	// go through all parents
			{
				Genome &genome = *subpop->parent_genomes_[i];
				int mutrun_count = genome.mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					MutationRun *mutrun = genome.mutruns_[run_index].get();
					int mut_count = mutrun->size();
					const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
					
					for (int mut_index = 0; mut_index < mut_count; ++mut_index)
					{
						Mutation *scan_mutation = mut_block_ptr + mut_ptr[mut_index];
						
						// use scratch_ to check whether the mutation is one we are outputting
						if (scan_mutation->scratch_)
							AddMutationToPolymorphismMap(&polymorphisms, scan_mutation);
					}
				}
			}
			
			// output the frequencies of these mutations in each subpopulation; note the format here comes from the old tracked mutations code
			// NOTE the format of this output changed because print_no_id() added the mutation_id_ to its output; BCH 11 June 2016
			for (const PolymorphismPair &polymorphism_pair : polymorphisms) 
			{ 
				out << "#OUT: " << generation_ << " T p" << subpop_pair.first << " ";
				polymorphism_pair.second.Print_NoID(out);
			}
		}
	}
	
	if (has_file)
		outfile.close(); 
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)outputUsage(void)
//
EidosValue_SP SLiMSim::ExecuteMethod_outputUsage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	std::ostream &out = p_interpreter.ExecutionOutputStream();
	
	// Save flags/precision and set to precision 1
	std::ios_base::fmtflags oldflags = out.flags();
	std::streamsize oldprecision = out.precision();
	out << std::fixed << std::setprecision(1);
	
	// Print header
	SLiM_MemoryUsage usage;
	
	TabulateMemoryUsage(&usage, &p_interpreter.SymbolTable());
	out << "Memory usage summary:" << std::endl;
	
	// Chromosome
	{
		assert(usage.chromosomeObjects_count == 1);
		
		out << "   Chromosome object: ";
		PrintBytes(out, usage.chromosomeObjects);
		
		out << "      Mutation rate maps: ";
		PrintBytes(out, usage.chromosomeMutationRateMaps);
		
		out << "      Recombination rate maps: ";
		PrintBytes(out, usage.chromosomeRecombinationRateMaps);
		
		out << "      Ancestral nucleotides: ";
		PrintBytes(out, usage.chromosomeAncestralSequence);
	}
	
	// Genome
	{
		out << "   Genome objects (" << usage.genomeObjects_count << "): ";
		PrintBytes(out, usage.genomeObjects);
		
		out << "      External MutationRun* buffers: ";
		PrintBytes(out, usage.genomeExternalBuffers);
		
		out << "      Unused pool space: ";
		PrintBytes(out, usage.genomeUnusedPoolSpace);
		
		out << "      Unused pool buffers: ";
		PrintBytes(out, usage.genomeUnusedPoolBuffers);
	}
	
	// GenomicElement
	{
		out << "   GenomicElement objects (" << usage.genomicElementObjects_count << "): ";
		PrintBytes(out, usage.genomicElementObjects);
	}
	
	// GenomicElementType
	{
		out << "   GenomicElementType objects (" << usage.genomicElementTypeObjects_count << "): ";
		PrintBytes(out, usage.genomicElementTypeObjects);
	}
	
	// Individual
	{
		out << "   Individual objects (" << usage.individualObjects_count << "): ";
		PrintBytes(out, usage.individualObjects);
		
		out << "      Unused pool space: ";
		PrintBytes(out, usage.individualUnusedPoolSpace);
	}
	
	// InteractionType
	{
		out << "   InteractionType objects (" << usage.interactionTypeObjects_count << "): ";
		PrintBytes(out, usage.interactionTypeObjects);
		
		if (usage.interactionTypeObjects_count)
		{
			out << "      k-d trees: ";
			PrintBytes(out, usage.interactionTypeKDTrees);
			
			out << "      Position caches: ";
			PrintBytes(out, usage.interactionTypePositionCaches);
			
			out << "      Sparse arrays: ";
			PrintBytes(out, usage.interactionTypeSparseArrays);
		}
	}
	
	// Mutation
	{
		out << "   Mutation objects (" << usage.mutationObjects_count << "): ";
		PrintBytes(out, usage.mutationObjects);
		
		out << "      Refcount buffer: ";
		PrintBytes(out, usage.mutationRefcountBuffer);
		
		out << "      Unused pool space: ";
		PrintBytes(out, usage.mutationUnusedPoolSpace);
	}
	
	// MutationRun
	{
		out << "   MutationRun objects (" << usage.mutationRunObjects_count << "): ";
		PrintBytes(out, usage.mutationRunObjects);
		
		out << "      External MutationIndex buffers: ";
		PrintBytes(out, usage.mutationRunExternalBuffers);
		
		out << "      Nonneutral mutation caches: ";
		PrintBytes(out, usage.mutationRunNonneutralCaches);
		
		out << "      Unused pool space: ";
		PrintBytes(out, usage.mutationRunUnusedPoolSpace);
		
		out << "      Unused pool buffers: ";
		PrintBytes(out, usage.mutationRunUnusedPoolBuffers);
	}
	
	// MutationType
	{
		out << "   MutationType objects (" << usage.mutationTypeObjects_count << "): ";
		PrintBytes(out, usage.mutationTypeObjects);
	}
	
	// SLiMSim (including the Population object)
	{
		assert(usage.slimsimObjects_count == 1);
		
		out << "   SLiMSim object: ";
		PrintBytes(out, usage.slimsimObjects);
		
		out << "      Tree-sequence tables: ";
		PrintBytes(out, usage.slimsimTreeSeqTables);
	}
	
	// Subpopulation
	{
		out << "   Subpopulation objects (" << usage.subpopulationObjects_count << "): ";
		PrintBytes(out, usage.subpopulationObjects);
		
		out << "      Fitness caches: ";
		PrintBytes(out, usage.subpopulationFitnessCaches);
		
		out << "      Parent tables: ";
		PrintBytes(out, usage.subpopulationParentTables);
		
		out << "      Spatial maps: ";
		PrintBytes(out, usage.subpopulationSpatialMaps);
		
		if (usage.subpopulationSpatialMapsDisplay)
		{
			out << "      Spatial map display (SLiMgui): ";
			PrintBytes(out, usage.subpopulationSpatialMapsDisplay);
		}
	}
	
	// Substitution
	{
		out << "   Substitution objects (" << usage.substitutionObjects_count << "): ";
		PrintBytes(out, usage.substitutionObjects);
	}
	
	// Eidos usage
	{
		out << "   Eidos: " << std::endl;
		
		out << "      EidosASTNode pool: ";
		PrintBytes(out, usage.eidosASTNodePool);
		
		out << "      EidosSymbolTable pool: ";
		PrintBytes(out, usage.eidosSymbolTablePool);
		
		out << "      EidosValue pool: ";
		PrintBytes(out, usage.eidosValuePool);
	}
	
	out << "   # Total accounted for: ";
	PrintBytes(out, usage.totalMemoryUsage);
	out << std::endl;
	
	// Restore flags/precision
	out.flags(oldflags);
	out.precision(oldprecision);
	
	return gStaticEidosValueVOID;
}

//	*********************	- (integer$)readFromPopulationFile(string$ filePath)
//
EidosValue_SP SLiMSim::ExecuteMethod_readFromPopulationFile(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMGenerationStage gen_stage = GenerationStage();
	
	if ((gen_stage != SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kWFStage5ExecuteLateScripts) &&
		(gen_stage != SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_readFromPopulationFile): readFromPopulationFile() may only be called from an early() or late() event." << EidosTerminate();
	if ((executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_readFromPopulationFile): readFromPopulationFile() may not be called from inside a callback." << EidosTerminate();
	
	if (!warned_early_read_)
	{
		if (GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (SLiMSim::ExecuteMethod_readFromPopulationFile): readFromPopulationFile() should probably not be called from an early() event in a WF model; fitness values will not be recalculated prior to offspring generation unless recalculateFitness() is called." << std::endl;
				warned_early_read_ = true;
			}
		}
		if (GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (SLiMSim::ExecuteMethod_readFromPopulationFile): readFromPopulationFile() should probably not be called from a late() event in a nonWF model; fitness values will not be recalculated prior to offspring generation unless recalculateFitness() is called." << std::endl;
				warned_early_read_ = true;
			}
		}
	}
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex(0, nullptr)));
	slim_generation_t file_generation = InitializePopulationFromFile(file_path, &p_interpreter);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(file_generation));
}
			
//	*********************	– (void)recalculateFitness([Ni$ generation = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_recalculateFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMGenerationStage gen_stage = GenerationStage();
	
	if ((gen_stage != SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kWFStage5ExecuteLateScripts) &&
		(gen_stage != SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_recalculateFitness): recalculateFitness() may only be called from an early() or late() event." << EidosTerminate();
	if ((executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_recalculateFitness): recalculateFitness() may not be called from inside a callback." << EidosTerminate();
	
	EidosValue *generation_value = p_arguments[0].get();
	
	// Trigger a fitness recalculation.  This is suggested after making a change that would modify fitness values, such as altering
	// a selection coefficient or dominance coefficient, changing the mutation type for a mutation, etc.  It will have the side
	// effect of calling fitness() callbacks, so this is quite a heavyweight operation.
	slim_generation_t gen = (generation_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(generation_value->IntAtIndex(0, nullptr)) : generation_;
	
	population_.RecalculateFitness(gen);
	
	return gStaticEidosValueVOID;
}

void SLiMSim::CheckScheduling(slim_generation_t p_target_gen, SLiMGenerationStage p_target_stage)
{
	if (p_target_gen < generation_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::CheckScheduling): event/callback scheduled for a past generation would not run." << EidosTerminate();
	if ((p_target_gen == generation_) && (p_target_stage < generation_stage_))
		EIDOS_TERMINATION << "ERROR (SLiMSim::CheckScheduling): event/callback scheduled for the current generation, but for a past generation cycle stage, would not run." << EidosTerminate();
	if ((p_target_gen == generation_) && (p_target_stage == generation_stage_))
		EIDOS_TERMINATION << "ERROR (SLiMSim::CheckScheduling): event/callback scheduled for the current generation, but for the currently executing generation cycle stage, would not run." << EidosTerminate();
}

//	*********************	– (object<SLiMEidosBlock>$)registerEarlyEvent(Nis$ id, string$ source, [Ni$ start = NULL], [Ni$ end = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerLateEvent(Nis$ id, string$ source, [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_registerEarlyLateEvent(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *start_value = p_arguments[2].get();
	EidosValue *end_value = p_arguments[3].get();
	
	slim_objectid_t script_id = -1;		// used if the id is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex(0, nullptr);
	slim_generation_t start_generation = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_generation_t end_generation = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (start_generation > end_generation)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerEarlyLateEvent): register" << ((p_method_id == gID_registerEarlyEvent) ? "Early" : "Late") << "Event() requires start <= end." << EidosTerminate();
	
	SLiMGenerationStage target_stage = (model_type_ == SLiMModelType::kModelTypeWF) ?
		((p_method_id == gID_registerEarlyEvent) ? SLiMGenerationStage::kWFStage1ExecuteEarlyScripts : SLiMGenerationStage::kWFStage5ExecuteLateScripts) : 
		((p_method_id == gID_registerEarlyEvent) ? SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts : SLiMGenerationStage::kNonWFStage6ExecuteLateScripts);
	
	CheckScheduling(start_generation, target_stage);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, (p_method_id == gID_registerEarlyEvent) ? SLiMEidosBlockType::SLiMEidosEventEarly : SLiMEidosBlockType::SLiMEidosEventLate, start_generation, end_generation);
	
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerFitnessCallback(Nis$ id, string$ source, Nio<MutationType>$ mutType, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_registerFitnessCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *mutType_value = p_arguments[2].get();
	EidosValue *subpop_value = p_arguments[3].get();
	EidosValue *start_value = p_arguments[4].get();
	EidosValue *end_value = p_arguments[5].get();
	
	slim_objectid_t script_id = -1;		// used if id_value is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex(0, nullptr);
	slim_objectid_t mut_type_id = -2;	// used if mutType_value is NULL, to indicate a global fitness() callback
	slim_objectid_t subpop_id = -1;		// used if subpop_value is NULL, to indicate applicability to all subpops
	slim_generation_t start_generation = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_generation_t end_generation = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (mutType_value->Type() != EidosValueType::kValueNULL)
		mut_type_id = (mutType_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(mutType_value->IntAtIndex(0, nullptr)) : ((MutationType *)mutType_value->ObjectElementAtIndex(0, nullptr))->mutation_type_id_;
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
	
	if (start_generation > end_generation)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerFitnessCallback): registerFitnessCallback() requires start <= end." << EidosTerminate();
	
	CheckScheduling(start_generation, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMGenerationStage::kWFStage6CalculateFitness : SLiMGenerationStage::kNonWFStage3CalculateFitness);
	
	SLiMEidosBlockType block_type = ((mut_type_id == -2) ? SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback : SLiMEidosBlockType::SLiMEidosFitnessCallback);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_generation, end_generation);
	
	new_script_block->mutation_type_id_ = mut_type_id;
	new_script_block->subpopulation_id_ = subpop_id;
	
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerInteractionCallback(Nis$ id, string$ source, io<InteractionType>$ intType, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_registerInteractionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
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
	slim_generation_t start_generation = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_generation_t end_generation = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
	
	if (start_generation > end_generation)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerInteractionCallback): registerInteractionCallback() requires start <= end." << EidosTerminate();
	
	CheckScheduling(start_generation, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMGenerationStage::kWFStage7AdvanceGenerationCounter : SLiMGenerationStage::kNonWFStage7AdvanceGenerationCounter);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, SLiMEidosBlockType::SLiMEidosInteractionCallback, start_generation, end_generation);
	
	new_script_block->interaction_type_id_ = int_type_id;
	new_script_block->subpopulation_id_ = subpop_id;
	
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerRecombinationCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_registerMateModifyRecCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (p_method_id == gID_registerMateChoiceCallback)
		if (ModelType() == SLiMModelType::kModelTypeNonWF)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerMateModifyRecCallback): method -registerMateChoiceCallback() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *subpop_value = p_arguments[2].get();
	EidosValue *start_value = p_arguments[3].get();
	EidosValue *end_value = p_arguments[4].get();
	
	slim_objectid_t script_id = -1;		// used if the id is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex(0, nullptr);
	slim_objectid_t subpop_id = -1;
	slim_generation_t start_generation = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_generation_t end_generation = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
	
	if (start_generation > end_generation)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerMateModifyRecCallback): " << Eidos_StringForGlobalStringID(p_method_id) << "() requires start <= end." << EidosTerminate();
	
	SLiMEidosBlockType block_type;
	
	if (p_method_id == gID_registerMateChoiceCallback)					block_type = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
	else if (p_method_id == gID_registerModifyChildCallback)			block_type = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
	else /* if (p_method_id == gID_registerRecombinationCallback) */	block_type = SLiMEidosBlockType::SLiMEidosRecombinationCallback;
	
	CheckScheduling(start_generation, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMGenerationStage::kWFStage2GenerateOffspring : SLiMGenerationStage::kNonWFStage1GenerateOffspring);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_generation, end_generation);
	
	new_script_block->subpopulation_id_ = subpop_id;
	
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerMutationCallback(Nis$ id, string$ source, [Nio<MutationType>$ mutType = NULL], [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_registerMutationCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *mutType_value = p_arguments[2].get();
	EidosValue *subpop_value = p_arguments[3].get();
	EidosValue *start_value = p_arguments[4].get();
	EidosValue *end_value = p_arguments[5].get();
	
	slim_objectid_t script_id = -1;		// used if id_value is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex(0, nullptr);
	slim_objectid_t mut_type_id = -1;	// used if mutType_value is NULL, to indicate applicability to all mutation types
	slim_objectid_t subpop_id = -1;		// used if subpop_value is NULL, to indicate applicability to all subpops
	slim_generation_t start_generation = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_generation_t end_generation = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (mutType_value->Type() != EidosValueType::kValueNULL)
		mut_type_id = (mutType_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(mutType_value->IntAtIndex(0, nullptr)) : ((MutationType *)mutType_value->ObjectElementAtIndex(0, nullptr))->mutation_type_id_;
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
	
	if (start_generation > end_generation)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerFitnessCallback): registerMutationCallback() requires start <= end." << EidosTerminate();
	
	CheckScheduling(start_generation, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMGenerationStage::kWFStage2GenerateOffspring : SLiMGenerationStage::kNonWFStage1GenerateOffspring);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, SLiMEidosBlockType::SLiMEidosMutationCallback, start_generation, end_generation);
	
	new_script_block->mutation_type_id_ = mut_type_id;
	new_script_block->subpopulation_id_ = subpop_id;
	
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerReproductionCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ns$ sex = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_registerReproductionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (ModelType() == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerReproductionCallback): method -registerReproductionCallback() is not available in WF models." << EidosTerminate();
	
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *subpop_value = p_arguments[2].get();
	EidosValue *sex_value = p_arguments[3].get();
	EidosValue *start_value = p_arguments[4].get();
	EidosValue *end_value = p_arguments[5].get();
	
	slim_objectid_t script_id = -1;		// used if the id is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex(0, nullptr);
	slim_objectid_t subpop_id = -1;
	IndividualSex sex_specificity = IndividualSex::kUnspecified;
	slim_generation_t start_generation = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(start_value->IntAtIndex(0, nullptr)) : 1);
	slim_generation_t end_generation = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToGenerationTypeOrRaise(end_value->IntAtIndex(0, nullptr)) : SLIM_MAX_GENERATION + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
	
	if (sex_value->Type() != EidosValueType::kValueNULL)
	{
		std::string sex_string = sex_value->StringAtIndex(0, nullptr);
		
		if (sex_string == "M")			sex_specificity = IndividualSex::kMale;
		else if (sex_string == "F")		sex_specificity = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerReproductionCallback): registerReproductionCallback() requires sex to be 'M', 'F', or NULL." << EidosTerminate();
		
		if (!sex_enabled_)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerReproductionCallback): registerReproductionCallback() requires sex to be NULL in non-sexual models." << EidosTerminate();
	}
	
	if (start_generation > end_generation)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_registerReproductionCallback): registerReproductionCallback() requires start <= end." << EidosTerminate();
	
	CheckScheduling(start_generation, SLiMGenerationStage::kNonWFStage1GenerateOffspring);
	
	SLiMEidosBlockType block_type = SLiMEidosBlockType::SLiMEidosReproductionCallback;
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_generation, end_generation);
	
	new_script_block->subpopulation_id_ = subpop_id;
	new_script_block->sex_specificity_ = sex_specificity;
	
	AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>)rescheduleScriptBlock(object<SLiMEidosBlock>$ block, [Ni$ start = NULL], [Ni$ end = NULL], [Ni generations = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_rescheduleScriptBlock(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *block_value = (EidosValue_Object *)p_arguments[0].get();
	EidosValue *start_value = p_arguments[1].get();
	EidosValue *end_value = p_arguments[2].get();
	EidosValue *generations_value = p_arguments[3].get();
	
	SLiMEidosBlock *block = (SLiMEidosBlock *)block_value->ObjectElementAtIndex(0, nullptr);
	bool start_null = (start_value->Type() == EidosValueType::kValueNULL);
	bool end_null = (end_value->Type() == EidosValueType::kValueNULL);
	bool generations_null = (generations_value->Type() == EidosValueType::kValueNULL);
	
	if (block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
	{
		// this should never be hit, because the user should have no way to get a reference to a function block
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_rescheduleScriptBlock): (internal error) rescheduleScriptBlock() cannot be called on user-defined function script blocks." << EidosTerminate();
	}
	
	// Figure out what generation stage the rescheduled block executes in; this is annoying, but necessary for the new scheduling check call
	SLiMGenerationStage stage;
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		switch (block->type_)
		{
			case SLiMEidosBlockType::SLiMEidosEventEarly:				stage = SLiMGenerationStage::kWFStage1ExecuteEarlyScripts; break;
			case SLiMEidosBlockType::SLiMEidosEventLate:				stage = SLiMGenerationStage::kWFStage5ExecuteLateScripts; break;
			case SLiMEidosBlockType::SLiMEidosInitializeCallback:		stage = SLiMGenerationStage::kStage0PreGeneration; break;
			case SLiMEidosBlockType::SLiMEidosFitnessCallback:			stage = SLiMGenerationStage::kWFStage6CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:	stage = SLiMGenerationStage::kWFStage6CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosInteractionCallback:		stage = SLiMGenerationStage::kWFStage7AdvanceGenerationCounter; break;
			case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		stage = SLiMGenerationStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		stage = SLiMGenerationStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	stage = SLiMGenerationStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosMutationCallback:			stage = SLiMGenerationStage::kWFStage2GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosReproductionCallback:		stage = SLiMGenerationStage::kWFStage2GenerateOffspring; break;
			default: EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_rescheduleScriptBlock): (internal error) rescheduleScriptBlock() cannot be called on this type of script block." << EidosTerminate();
		}
	}
	else
	{
		switch (block->type_)
		{
			case SLiMEidosBlockType::SLiMEidosEventEarly:				stage = SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts; break;
			case SLiMEidosBlockType::SLiMEidosEventLate:				stage = SLiMGenerationStage::kNonWFStage6ExecuteLateScripts; break;
			case SLiMEidosBlockType::SLiMEidosInitializeCallback:		stage = SLiMGenerationStage::kStage0PreGeneration; break;
			case SLiMEidosBlockType::SLiMEidosFitnessCallback:			stage = SLiMGenerationStage::kNonWFStage3CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:	stage = SLiMGenerationStage::kNonWFStage3CalculateFitness; break;
			case SLiMEidosBlockType::SLiMEidosInteractionCallback:		stage = SLiMGenerationStage::kNonWFStage7AdvanceGenerationCounter; break;
			case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:		stage = SLiMGenerationStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosModifyChildCallback:		stage = SLiMGenerationStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosRecombinationCallback:	stage = SLiMGenerationStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosMutationCallback:			stage = SLiMGenerationStage::kNonWFStage1GenerateOffspring; break;
			case SLiMEidosBlockType::SLiMEidosReproductionCallback:		stage = SLiMGenerationStage::kNonWFStage1GenerateOffspring; break;
			default: EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_rescheduleScriptBlock): (internal error) rescheduleScriptBlock() cannot be called on this type of script block." << EidosTerminate();
		}
	}
	
	if ((!start_null || !end_null) && generations_null)
	{
		// start/end case; this is simple
		
		slim_generation_t start = (start_null ? 1 : SLiMCastToGenerationTypeOrRaise(start_value->IntAtIndex(0, nullptr)));
		slim_generation_t end = (end_null ? SLIM_MAX_GENERATION + 1 : SLiMCastToGenerationTypeOrRaise(end_value->IntAtIndex(0, nullptr)));
		
		if (start > end)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_rescheduleScriptBlock): reschedule() requires start <= end." << EidosTerminate();
		
		CheckScheduling(start, stage);
		
		block->start_generation_ = start;
		block->end_generation_ = end;
		last_script_block_gen_cached_ = false;
		script_block_types_cached_ = false;
		scripts_changed_ = true;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(block, gSLiM_SLiMEidosBlock_Class));
	}
	else if (!generations_null && (start_null && end_null))
	{
		// generations case; this is complicated
		
		// first, fetch the generations and make sure they are in bounds
		std::vector<slim_generation_t> generations;
		int gen_count = generations_value->Count();
		
		if (gen_count < 1)
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_rescheduleScriptBlock): reschedule() requires at least one generation; use deregisterScriptBlock() to remove a script block from the simulation." << EidosTerminate();
		
		generations.reserve(gen_count);
		
		for (int gen_index = 0; gen_index < gen_count; ++gen_index)
			generations.push_back(SLiMCastToGenerationTypeOrRaise(generations_value->IntAtIndex(gen_index, nullptr)));
		
		// next, sort the generation list and check that the first scheduling it requests is not in the past
		std::sort(generations.begin(), generations.end());
		
		CheckScheduling(generations.front(), stage);
		
		// finally, go through the generation vector and schedule blocks for sequential runs
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_SLiMEidosBlock_Class));
		EidosValue_SP result_SP = EidosValue_SP(vec);
		bool first_block = true;
		
		slim_generation_t start = -10;
		slim_generation_t end = -10;
		int gen_index = 0;
		
		// I'm sure there's a prettier algorithm for finding the sequential runs, but I'm not seeing it right now.
		// The tricky thing is that I want there to be only a single place in the code where a block is scheduled;
		// it seems easy to write a version where blocks get scheduled in two places, a main case and a tail case.
		while (true)
		{
			slim_generation_t gen = generations[gen_index];
			bool reached_end_in_seq = false;
			
			if (gen == end + 1)			// sequential value seen; move on to the next sequential value
			{
				end++;
				
				if (++gen_index < gen_count)
					continue;
				reached_end_in_seq = true;
			}
			else if (gen <= end)
			{
				EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_rescheduleScriptBlock): reschedule() requires that the generation vector contain unique values; the same generation cannot be used twice." << EidosTerminate();
			}
			
			// make new block and move on to start the next sequence
		makeBlock:
			if ((start != -10) && (end != -10))
			{
				// start and end define the range of the block to schedule; first_block
				// determines whether we use the existing block or make a new one
				if (first_block)
				{
					block->start_generation_ = start;
					block->end_generation_ = end;
					first_block = false;
					last_script_block_gen_cached_ = false;
					script_block_types_cached_ = false;
					scripts_changed_ = true;
					
					vec->push_object_element_NORR(block);
				}
				else
				{
					SLiMEidosBlock *new_script_block = new SLiMEidosBlock(-1, block->compound_statement_node_->token_->token_string_, block->type_, start, end);
					
					AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
					
					vec->push_object_element_NORR(new_script_block);
				}
			}
			
			start = gen;
			end = gen;
			++gen_index;
			
			if ((gen_index == gen_count) && !reached_end_in_seq)
				goto makeBlock;
			else if (gen_index >= gen_count)
				break;
		}
		
		return result_SP;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_rescheduleScriptBlock): reschedule() requires that either start/end or generations be supplied, but not both." << EidosTerminate();
	}
}

//	*********************	- (void)simulationFinished(void)
//
EidosValue_SP SLiMSim::ExecuteMethod_simulationFinished(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	sim_declared_finished_ = true;
	
	return gStaticEidosValueVOID;
}

//	*********************	- (object<Mutation>)subsetMutations([No<Mutation>$ exclude = NULL], [Nio<MutationType>$ mutationType = NULL], [Ni$ position = NULL], [Nis$ nucleotide = NULL], [Ni$ tag = NULL], [Ni$ id = NULL])
//
EidosValue_SP SLiMSim::ExecuteMethod_subsetMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *exclude_value = p_arguments[0].get();
	EidosValue *mutType_value = p_arguments[1].get();
	EidosValue *position_value = p_arguments[2].get();
	EidosValue *nucleotide_value = p_arguments[3].get();
	EidosValue *tag_value = p_arguments[4].get();
	EidosValue *id_value = p_arguments[5].get();
	
	// parse our arguments
	Mutation *exclude = (exclude_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Mutation *)exclude_value->ObjectElementAtIndex(0, nullptr);
	MutationType *mutation_type_ptr = (mutType_value->Type() == EidosValueType::kValueNULL) ? nullptr : SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, *this, "subsetMutations()");
	slim_position_t position = (position_value->Type() == EidosValueType::kValueNULL) ? -1 : SLiMCastToPositionTypeOrRaise(position_value->IntAtIndex(0, nullptr));
	int8_t nucleotide = -1;
	bool has_tag = !(tag_value->Type() == EidosValueType::kValueNULL);
	slim_usertag_t tag = (has_tag ? tag_value->IntAtIndex(0, nullptr) : 0);
	bool has_id = !(id_value->Type() == EidosValueType::kValueNULL);
	slim_mutationid_t id = (has_id ? id_value->IntAtIndex(0, nullptr) : 0);
	
	if (nucleotide_value->Type() == EidosValueType::kValueInt)
	{
		int64_t nuc_int = nucleotide_value->IntAtIndex(0, nullptr);
		
		if ((nuc_int < 0) || (nuc_int > 3))
			EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_subsetMutations): subsetMutations() requires integer nucleotide values to be in [0,3]." << EidosTerminate();
		
		nucleotide = (int8_t)nuc_int;
	}
	else if (nucleotide_value->Type() == EidosValueType::kValueString)
	{
		std::string nuc_string = nucleotide_value->StringAtIndex(0, nullptr);
		
		if (nuc_string == "A")		nucleotide = 0;
		else if (nuc_string == "C")	nucleotide = 1;
		else if (nuc_string == "G")	nucleotide = 2;
		else if (nuc_string == "T")	nucleotide = 3;
		else EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_subsetMutations): subsetMutations() requires string nucleotide values to be 'A', 'C', 'G', or 'T'." << EidosTerminate();
	}
	
	// We will scan forward looking for a match, and will keep track of the first match we find.  If we only find one, we return
	// a singleton; if we find a second, we will start accumulating a vector result.
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int registry_size;
	const MutationIndex *registry = population_.MutationRegistry(&registry_size);
	int match_count = 0, registry_index;
	Mutation *first_match = nullptr;
	EidosValue_Object_vector *vec = nullptr;
	
	if (has_id && !exclude && !mutation_type_ptr && (position == -1) && (nucleotide == -1) && !has_tag)
	{
		// id-only search; nice for this to be fast since people will use it to look up a specific mutation
		for (registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			Mutation *mut = mut_block_ptr + registry[registry_index];
			
			if (mut->mutation_id_ != id)
				continue;
			
			match_count++;
			
			if (match_count == 1)
			{
				first_match = mut;
			}
			else if (match_count == 2)
			{
				vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
				vec->push_object_element_RR(first_match);
				vec->push_object_element_RR(mut);
			}
			else
			{
				vec->push_object_element_RR(mut);
			}
		}
	}
	else if (!exclude && !has_tag && !has_id)
	{
		// no exclude, tag, or id; this is expected to be the common case, for the usage patterns I anticipate
		for (registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			Mutation *mut = mut_block_ptr + registry[registry_index];
			
			if (mutation_type_ptr && (mut->mutation_type_ptr_ != mutation_type_ptr))	continue;
			if ((position != -1) && (mut->position_ != position))						continue;
			if ((nucleotide != -1) && (mut->nucleotide_ != nucleotide))					continue;
			
			match_count++;
			
			if (match_count == 1)
			{
				first_match = mut;
			}
			else if (match_count == 2)
			{
				vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
				vec->push_object_element_RR(first_match);
				vec->push_object_element_RR(mut);
			}
			else
			{
				vec->push_object_element_RR(mut);
			}
		}
	}
	else
	{
		// GENERAL CASE
		for (registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			Mutation *mut = mut_block_ptr + registry[registry_index];
			
			if (exclude && (mut == exclude))											continue;
			if (mutation_type_ptr && (mut->mutation_type_ptr_ != mutation_type_ptr))	continue;
			if ((position != -1) && (mut->position_ != position))						continue;
			if ((nucleotide != -1) && (mut->nucleotide_ != nucleotide))					continue;
			if (has_tag && (mut->tag_value_ != tag))									continue;
			if (has_id && (mut->mutation_id_ != id))									continue;
			
			match_count++;
			
			if (match_count == 1)
			{
				first_match = mut;
			}
			else if (match_count == 2)
			{
				vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
				vec->push_object_element_RR(first_match);
				vec->push_object_element_RR(mut);
			}
			else
			{
				vec->push_object_element_RR(mut);
			}
		}
	}
	
	if (match_count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
	else if (match_count == 1)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(first_match, gSLiM_Mutation_Class));
	else
		return EidosValue_SP(vec);
}

// TREE SEQUENCE RECORDING
//	*********************	- (logical$)treeSeqCoalesced(void)
//
EidosValue_SP SLiMSim::ExecuteMethod_treeSeqCoalesced(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqCoalesced): treeSeqCoalesced() may only be called when tree recording is enabled." << EidosTerminate();
	if (!running_coalescence_checks_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqCoalesced): treeSeqCoalesced() may only be called when coalescence checking is enabled; pass checkCoalescence=T to initializeTreeSeq() to enable this feature." << EidosTerminate();
	
	return (last_coalescence_state_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

// TREE SEQUENCE RECORDING
//	*********************	- (void)treeSeqSimplify(void)
//
EidosValue_SP SLiMSim::ExecuteMethod_treeSeqSimplify(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqSimplify): treeSeqSimplify() may only be called when tree recording is enabled." << EidosTerminate();
	
	SLiMGenerationStage gen_stage = GenerationStage();
	
	if ((gen_stage != SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kWFStage5ExecuteLateScripts) &&
		(gen_stage != SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqSimplify): treeSeqSimplify() may only be called from an early() or late() event." << EidosTerminate();
	if ((executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqSimplify): treeSeqSimplify() may not be called from inside a callback." << EidosTerminate();
	
	SimplifyTreeSequence();
	
	return gStaticEidosValueVOID;
}

// TREE SEQUENCE RECORDING
//	*********************	- (void)treeSeqRememberIndividuals(object<Individual> individuals)
//
EidosValue_SP SLiMSim::ExecuteMethod_treeSeqRememberIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue_Object *individuals_value = (EidosValue_Object *)p_arguments[0].get();
	int ind_count = individuals_value->Count();
	
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqRememberIndividuals): treeSeqRememberIndividuals() may only be called when tree recording is enabled." << EidosTerminate();
	
	// BCH 14 November 2018: removed a block on calling treeSeqRememberIndividuals() from fitness() callbacks,
	// because it turns out that can be useful (see correspondence with Yan Wong)
	// BCH 30 April 2019: also allowing mutation() callbacks, since I can see how that could be useful...
	if ((executing_block_type_ == SLiMEidosBlockType::SLiMEidosMateChoiceCallback) || (executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback) || (executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqRememberIndividuals): treeSeqRememberIndividuals() may not be called from inside a mateChoice(), modifyChild(), or recombination() callback." << EidosTerminate();
	
	if (individuals_value->Count() == 1)
	{
		Individual *ind = (Individual *)individuals_value->ObjectElementAtIndex(0, nullptr);
		AddIndividualsToTable(&ind, 1, &tables_, SLIM_TSK_INDIVIDUAL_REMEMBERED);
	}
	else
	{
		const EidosValue_Object_vector *ind_vector = individuals_value->ObjectElementVector();
		EidosObjectElement * const *oe_buffer = ind_vector->data();
		Individual * const *ind_buffer = (Individual * const *)oe_buffer;
		AddIndividualsToTable(ind_buffer, ind_count, &tables_, SLIM_TSK_INDIVIDUAL_REMEMBERED);
	}
	
	return gStaticEidosValueVOID;
}

// TREE SEQUENCE RECORDING
//	*********************	- (void)treeSeqOutput(string$ path, [logical$ simplify = T], [logical$ includeModel = T], [logical$ _binary = T]) (note the _binary flag is undocumented)
//
EidosValue_SP SLiMSim::ExecuteMethod_treeSeqOutput(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *path_value = p_arguments[0].get();
	EidosValue *simplify_value = p_arguments[1].get();
	EidosValue *includeModel_value = p_arguments[2].get();
	EidosValue *binary_value = p_arguments[3].get();
	
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqOutput): treeSeqOutput() may only be called when tree recording is enabled." << EidosTerminate();
	
	SLiMGenerationStage gen_stage = GenerationStage();
	
	if ((gen_stage != SLiMGenerationStage::kWFStage1ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kWFStage5ExecuteLateScripts) &&
		(gen_stage != SLiMGenerationStage::kNonWFStage2ExecuteEarlyScripts) && (gen_stage != SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqOutput): treeSeqOutput() may only be called from an early() or late() event." << EidosTerminate();
	if ((executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteMethod_treeSeqOutput): treeSeqOutput() may not be called from inside a callback." << EidosTerminate();
	
	std::string path_string = path_value->StringAtIndex(0, nullptr);
	bool binary = binary_value->LogicalAtIndex(0, nullptr);
	bool simplify = simplify_value->LogicalAtIndex(0, nullptr);
	bool includeModel = includeModel_value->LogicalAtIndex(0, nullptr);
	
	WriteTreeSequence(path_string, binary, simplify, includeModel);
	
	return gStaticEidosValueVOID;
}


//
//	SLiMSim_Class
//
#pragma mark -
#pragma mark SLiMSim_Class
#pragma mark -

class SLiMSim_Class : public EidosDictionary_Class
{
public:
	SLiMSim_Class(const SLiMSim_Class &p_original) = delete;	// no copy-construct
	SLiMSim_Class& operator=(const SLiMSim_Class&) = delete;	// no copying
	inline SLiMSim_Class(void) { }
	
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};

EidosObjectClass *gSLiM_SLiMSim_Class = new SLiMSim_Class();


const std::string &SLiMSim_Class::ElementType(void) const
{
	return gStr_SLiMSim;
}

const std::vector<EidosPropertySignature_CSP> *SLiMSim_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*EidosDictionary_Class::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosomeType,			true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_dimensionality,			true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_periodicity,			true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElementTypes,	true,	kEidosValueMaskObject, gSLiM_GenomicElementType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_inSLiMgui,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_interactionTypes,		true,	kEidosValueMaskObject, gSLiM_InteractionType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_modelType,				true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutations,				true,	kEidosValueMaskObject, gSLiM_Mutation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationTypes,			true,	kEidosValueMaskObject, gSLiM_MutationType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideBased,		true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_scriptBlocks,			true,	kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sexEnabled,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulations,			true,	kEidosValueMaskObject, gSLiM_Subpopulation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_substitutions,			true,	kEidosValueMaskObject, gSLiM_Substitution_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_dominanceCoeffX,		false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_generation,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *SLiMSim_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*EidosDictionary_Class::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSubpop, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->AddIntString_S("subpopID")->AddInt_S("size")->AddFloat_OS("sexRatio", gStaticEidosValue_Float0Point5));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSubpopSplit, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->AddIntString_S("subpopID")->AddInt_S("size")->AddIntObject_S("sourceSubpop", gSLiM_Subpopulation_Class)->AddFloat_OS("sexRatio", gStaticEidosValue_Float0Point5));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_deregisterScriptBlock, kEidosValueMaskVOID))->AddIntObject("scriptBlocks", gSLiM_SLiMEidosBlock_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationFrequencies, kEidosValueMaskFloat))->AddObject_N("subpops", gSLiM_Subpopulation_Class)->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationCounts, kEidosValueMaskInt))->AddObject_N("subpops", gSLiM_Subpopulation_Class)->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputFixedMutations, kEidosValueMaskVOID))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputFull, kEidosValueMaskVOID))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("binary", gStaticEidosValue_LogicalF)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("spatialPositions", gStaticEidosValue_LogicalT)->AddLogical_OS("ages", gStaticEidosValue_LogicalT)->AddLogical_OS("ancestralNucleotides", gStaticEidosValue_LogicalT)->AddLogical_OS("pedigreeIDs", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMutations, kEidosValueMaskVOID))->AddObject("mutations", gSLiM_Mutation_Class)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputUsage, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_readFromPopulationFile, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddString_S("filePath"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_recalculateFitness, kEidosValueMaskVOID))->AddInt_OSN("generation", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerEarlyEvent, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerLateEvent, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerFitnessCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_SN("mutType", gSLiM_MutationType_Class)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerInteractionCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_S("intType", gSLiM_InteractionType_Class)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerMateChoiceCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerModifyChildCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerRecombinationCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerMutationCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_OSN("mutType", gSLiM_MutationType_Class, gStaticEidosValueNULL)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerReproductionCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S("source")->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddString_OSN("sex", gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_rescheduleScriptBlock, kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class))->AddObject_S("block", gSLiM_SLiMEidosBlock_Class)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL)->AddInt_ON("generations", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_simulationFinished, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_subsetMutations, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddObject_OSN("exclude", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddIntObject_OSN("mutType", gSLiM_MutationType_Class, gStaticEidosValueNULL)->AddInt_OSN("position", gStaticEidosValueNULL)->AddIntString_OSN("nucleotide", gStaticEidosValueNULL)->AddInt_OSN("tag", gStaticEidosValueNULL)->AddInt_OSN("id", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqCoalesced, kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqSimplify, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqRememberIndividuals, kEidosValueMaskVOID))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqOutput, kEidosValueMaskVOID))->AddString_S("path")->AddLogical_OS("simplify", gStaticEidosValue_LogicalT)->AddLogical_OS("includeModel", gStaticEidosValue_LogicalT)->AddLogical_OS("_binary", gStaticEidosValue_LogicalT));
							  
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
