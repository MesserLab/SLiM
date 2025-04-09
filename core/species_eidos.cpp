//
//  species_eidos.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020-2025 Philipp Messer.  All rights reserved.
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


#include "species.h"

#include "community.h"
#include "haplosome.h"
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


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

// Note that the functions below are dispatched out by Community::ContextDefinedFunctionDispatch()

//	*********************	(integer$)initializeAncestralNucleotides(is sequence)
//
EidosValue_SP Species::ExecuteContextFunction_initializeAncestralNucleotides(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *sequence_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_ancseq_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeAncestralNucleotides): initializeAncestralNucleotides() may be called only once." << EidosTerminate();
	if (!nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeAncestralNucleotides): initializeAncestralNucleotides() may be only be called in nucleotide-based models." << EidosTerminate();
	
	EidosValueType sequence_value_type = sequence_value->Type();
	int sequence_value_count = sequence_value->Count();
	
	if (sequence_value_count == 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeAncestralNucleotides): initializeAncestralNucleotides() requires a sequence of length >= 1." << EidosTerminate();
	
	// This function triggers the creation of an implicit chromosome if a chromosome has not already been set up
	if ((num_chromosome_inits_ == 0) && !has_implicit_chromosome_)
		MakeImplicitChromosome(ChromosomeType::kA_DiploidAutosome);
	
	Chromosome *chromosome = CurrentlyInitializingChromosome();
	
	if (sequence_value_type == EidosValueType::kValueInt)
	{
		// A vector of integers has been provided, where ACGT == 0123
		const int64_t *int_data = sequence_value->IntData();
		
		chromosome->ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, int_data);
	}
	else if (sequence_value_type == EidosValueType::kValueString)
	{
		if (sequence_value_count != 1)
		{
			// A vector of characters has been provided, which must all be "A" / "C" / "G" / "T"
			const std::string *string_data = sequence_value->StringData();
			
			chromosome->ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, string_data);
		}
		else	// sequence_value_count == 1
		{
			const std::string &sequence_string = sequence_value->StringData()[0];
			bool contains_only_nuc = true;
			
			// OK, we do a weird thing here.  We want to try to construct a NucleotideArray
			// from sequence_string, which throws with EIDOS_TERMINATION if it fails, but
			// we want to actually catch that exception even if we're running at the
			// command line, where EIDOS_TERMINATION normally calls exit().  So we actually
			// play around with the error-handling state to make it do what we want it to do.
			// This is very naughty and should be redesigned, but right now I'm not seeing
			// the right redesign strategy, so... hacking it for now.  Parallel code is at
			// Chromosome::ExecuteMethod_setAncestralNucleotides()
			bool save_gEidosTerminateThrows = gEidosTerminateThrows;
			gEidosTerminateThrows = true;
			
			try {
				chromosome->ancestral_seq_buffer_ = new NucleotideArray(sequence_string.length(), sequence_string.c_str());
			} catch (...) {
				contains_only_nuc = false;
				
				// clean up the error state since we don't want this throw to be reported
				gEidosTermination.clear();
				gEidosTermination.str("");
			}
			
			gEidosTerminateThrows = save_gEidosTerminateThrows;
			
			if (!contains_only_nuc)
			{
				// A singleton string has been provided that contains characters other than ACGT; we will interpret it as a filesystem path for a FASTA file
				std::string file_path = Eidos_ResolvedPath(sequence_string);
				std::ifstream file_stream(file_path.c_str());
				
				if (!file_stream.is_open())
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeAncestralNucleotides): the file at path " << sequence_string << " could not be opened or does not exist." << EidosTerminate();
				
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
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeAncestralNucleotides): a filesystem error occurred while reading the file at path " << sequence_string << "." << EidosTerminate();
				
				if (fasta_sequence.length() == 0)
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeAncestralNucleotides): no FASTA sequence found in " << sequence_string << "." << EidosTerminate();
				
				chromosome->ancestral_seq_buffer_ = new NucleotideArray(fasta_sequence.length(), fasta_sequence.c_str());
			}
		}
	}
	
	if (chromosome->extent_immutable_)
	{
		if (chromosome->ancestral_seq_buffer_->size() != (std::size_t)(chromosome->last_position_ + 1))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeAncestralNucleotides): the length of the provided ancestral sequence does not match the length of the chromosome." << EidosTerminate();
	}
	
	// debugging
	//std::cout << "ancestral sequence set: " << *chromosome_->ancestral_seq_buffer_ << std::endl;
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeAncestralNucleotides(\"";
		
		// output up to 20 nucleotides, followed by an ellipsis if necessary
		for (std::size_t i = 0; (i < 20) && (i < chromosome->ancestral_seq_buffer_->size()); ++i)
			output_stream << "ACGT"[chromosome->ancestral_seq_buffer_->NucleotideAtIndex(i)];
		
		if (chromosome->ancestral_seq_buffer_->size() > 20)
			output_stream << gEidosStr_ELLIPSIS;
		
		output_stream << "\");" << std::endl;
	}
	
	num_ancseq_inits_++;
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(chromosome->ancestral_seq_buffer_->size()));
}

//	*********************	(object<Chromosome>$)initializeChromosome(integer$ id, [Ni$ length = NULL], [string$ type = "A"], [Ns$ symbol = NULL], [Ns$ name = NULL], [integer$ mutationRuns = 0])
//
EidosValue_SP Species::ExecuteContextFunction_initializeChromosome(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	// We are starting the definition of a new explicitly defined chromosome.  We zero out counts for all
	// chromosome-specific initialization functions; this is a blank slate.  An implicit chromosome is
	// not allowed to have already been defined.
	if (has_implicit_chromosome_)
	{
		if (num_mutrate_inits_ > 0)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() cannot be called to explicitly create a chromosome, because the chromosome has already been implicitly defined.  This occurred because initializeMutationRate() was called.  To fix this error, call initializeChromosome() first and then call initializeMutationRate(), or don't call initializeChromosome() at all if you do not need an explicitly defined chromosome." << EidosTerminate();
		if (num_recrate_inits_ > 0)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() cannot be called to explicitly create a chromosome, because the chromosome has already been implicitly defined.  This occurred because initializeRecombinationRate() was called.  To fix this error, call initializeChromosome() first and then call initializeRecombinationRate(), or don't call initializeChromosome() at all if you do not need an explicitly defined chromosome." << EidosTerminate();
		if (num_genomic_element_inits_ > 0)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() cannot be called to explicitly create a chromosome, because the chromosome has already been implicitly defined.  This occurred because initializeGenomicElement() was called.  To fix this error, call initializeChromosome() first and then call initializeGenomicElement(), or don't call initializeChromosome() at all if you do not need an explicitly defined chromosome." << EidosTerminate();
		if (num_gene_conv_inits_ > 0)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() cannot be called to explicitly create a chromosome, because the chromosome has already been implicitly defined.  This occurred because initializeGeneConversion() was called.  To fix this error, call initializeChromosome() first and then call initializeGeneConversion(), or don't call initializeChromosome() at all if you do not need an explicitly defined chromosome." << EidosTerminate();
		if (num_ancseq_inits_ > 0)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() cannot be called to explicitly create a chromosome, because the chromosome has already been implicitly defined.  This occurred because initializeAncestralNucleotides() was called.  To fix this error, call initializeChromosome() first and then call initializeAncestralNucleotides(), or don't call initializeChromosome() at all if you do not need an explicitly defined chromosome." << EidosTerminate();
		if (num_hotmap_inits_ > 0)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() cannot be called to explicitly create a chromosome, because the chromosome has already been implicitly defined.  This occurred because initializeHotspotMap() was called.  To fix this error, call initializeChromosome() first and then call initializeHotspotMap(), or don't call initializeChromosome() at all if you do not need an explicitly defined chromosome." << EidosTerminate();
		
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): (internal error) initializeChromosome() was called with an implicitly defined chromosome.  However, the cause of this cannot be diagnosed, indicating an internal logic error." << EidosTerminate();
	}
	
	if (chromosomes_.size() >= SLIM_MAX_CHROMOSOMES)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() cannot make a new chromosome because the maximum number of chromosomes allowed per species (" << SLIM_MAX_CHROMOSOMES << ") has already been reached.  If you want to model a large number of unlinked loci, using a recombination rate of 0.5, rather than multiple chromosomes, is recommended." << EidosTerminate();
	
	if (num_chromosome_inits_ > 0)
	{
		// A previous explicitly defined chromosome terminates its definition here,
		// so we do some checking of that previous chromosome's integrity.
		EndCurrentChromosome(/* starting_new_chromosome */ true);
	}
	
	num_mutrate_inits_ = 0;
	num_recrate_inits_ = 0;
	num_genomic_element_inits_ = 0;
	num_gene_conv_inits_ = 0;
	num_ancseq_inits_ = 0;
	num_hotmap_inits_ = 0;
	
	// Get parameters and bounds-check
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *length_value = p_arguments[1].get();
	EidosValue *type_value = p_arguments[2].get();
	EidosValue *symbol_value = p_arguments[3].get();
	EidosValue *name_value = p_arguments[4].get();
	EidosValue *mutationRuns_value = p_arguments[5].get();
	
	int64_t id = id_value->IntAtIndex_NOCAST(0, nullptr);
	
	if (id < 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires id to be non-negative." << EidosTerminate();
	
	if (ChromosomeFromID(id))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires id to be unique within the species; two chromosomes in the same species may not have the same id." << EidosTerminate();
	
	// -1 represents a length of NULL, indicating the length is mutable and will be assessed later
	slim_position_t length = -1;	
	
	if (length_value->Type() == EidosValueType::kValueInt)
	{
		SLiMCastToPositionTypeOrRaise(length_value->IntAtIndex_NOCAST(0, nullptr));
		
		if (length - 1 > SLIM_MAX_BASE_POSITION)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires the last base position (length-1) to be <= 1e15." << EidosTerminate();
	}
	
	std::string type_string = type_value->StringAtIndex_NOCAST(0, nullptr);
	ChromosomeType chromosome_type = ChromosomeTypeForString(type_string);
	
	if (!sex_enabled_ &&
		((chromosome_type == ChromosomeType::kX_XSexChromosome) ||
		 (chromosome_type == ChromosomeType::kY_YSexChromosome) ||
		 (chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
		 (chromosome_type == ChromosomeType::kW_WSexChromosome) ||
		 (chromosome_type == ChromosomeType::kHF_HaploidFemaleInherited) ||
		 (chromosome_type == ChromosomeType::kFL_HaploidFemaleLine) ||
		 (chromosome_type == ChromosomeType::kHM_HaploidMaleInherited) ||
		 (chromosome_type == ChromosomeType::kML_HaploidMaleLine) ||
		 (chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull)))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): chromosome type '" << chromosome_type << "' is only allowed in sexual models; call initializeSex() to enable sex first." << EidosTerminate();
	
	std::string symbol;
	
	if (symbol_value->Type() == EidosValueType::kValueString)
		symbol = symbol_value->StringAtIndex_NOCAST(0, nullptr);
	else
		symbol = std::to_string(id);
	
	if ((symbol.length() == 0) || (symbol.length() > 5))
	{
		if (symbol_value->Type() == EidosValueType::kValueString)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires symbol to be a string with a length of 1-3 characters." << EidosTerminate();
		else
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires symbol to be a string with a length of 1-3 characters; since the id given to the chromosome (" << id << ") is more than three digits, a symbol must be supplied explicitly to satisfy this requirement." << EidosTerminate();
	}
	
	// these checks for symbol try to ensure that it can be used in a filename, as in tree-seq recording, without causing problems
	for (char c : symbol) {
		if (!std::isprint(static_cast<unsigned char>(c))) {
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires symbol to consist only of printable ASCII characters." << EidosTerminate();
		}
	}
	if (symbol.find_first_of(" \\/:$*?<>|._-\"") != std::string::npos)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() does not allow symbol to contain the characters [space], \\, /, :, $, *, ?, <, >, |, ., _, -, or \"." << EidosTerminate();
	
	if (ChromosomeFromSymbol(symbol))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires symbol to be unique within the species; two chromosomes in the same species may not have the same symbol." << EidosTerminate();
	
	std::string name;
	
	if (name_value->Type() == EidosValueType::kValueString)
		name = name_value->StringAtIndex_NOCAST(0, nullptr);
	
	int64_t mutrun_count = mutationRuns_value->IntAtIndex_NOCAST(0, nullptr);
	
	if (mutrun_count != 0)
	{
		if ((mutrun_count < 1) || (mutrun_count > 10000))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeChromosome): initializeChromosome() requires mutationRuns to be between 1 and 10000, inclusive." << EidosTerminate();
	}
	
	// Set up the new chromosome object; it gets a retain count on it from EidosDictionaryRetained::EidosDictionaryRetained()
	Chromosome *chromosome = new Chromosome(*this, chromosome_type, id, symbol, /* p_index */ (uint8_t)num_chromosome_inits_, (int)mutrun_count);
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(chromosome, gSLiM_Chromosome_Class));
	
	chromosome->SetName(name);
	
	if (length == -1)
	{
		// the length is NULL, so it is mutable until Chromosome::InitializeDraws() is called
		chromosome->last_position_ = 0;
		chromosome->extent_immutable_ = false;
	}
	else
	{
		// the length has been specified explicitly, so it is immutable
		chromosome->last_position_ = length - 1;
		chromosome->extent_immutable_ = true;
	}
	
	// Add it to our registry; AddChromosome() takes its retain count
	AddChromosome(chromosome);
	num_chromosome_inits_++;
	has_currently_initializing_chromosome_ = true;
	
	if (SLiM_verbosity_level >= 1)
	{
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		output_stream << "initializeChromosome(" << id << ", " << length << ", '" << type_string << "'";
		if (symbol_value->Type() == EidosValueType::kValueString)
			output_stream << ", symbol='" << symbol << "'";
		if (name.length())
			output_stream << ", name='" << name << "'";
		if (mutrun_count != 0)
			output_stream << ", mutationRuns=" << mutrun_count;
		output_stream << ");" << std::endl;
	}
	
	return result_SP;
}

//	*********************	(object<GenomicElement>)initializeGenomicElement(io<GenomicElementType> genomicElementType, integer start, integer end)
//
EidosValue_SP Species::ExecuteContextFunction_initializeGenomicElement(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *genomicElementType_value = p_arguments[0].get();
	EidosValue *start_value = p_arguments[1].get();
	EidosValue *end_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (start_value->Count() != end_value->Count())
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() requires start and end to be the same length." << EidosTerminate();
	if ((genomicElementType_value->Count() != 1) && (genomicElementType_value->Count() != start_value->Count()))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() requires genomicElementType to be a singleton, or to match the length of start and end." << EidosTerminate();
	
	int element_count = start_value->Count();
	int type_count = genomicElementType_value->Count();
	
	if (element_count == 0)
		return gStaticEidosValueVOID;
	
	// This function triggers the creation of an implicit chromosome if a chromosome has not already been set up
	if ((num_chromosome_inits_ == 0) && !has_implicit_chromosome_)
		MakeImplicitChromosome(ChromosomeType::kA_DiploidAutosome);
	
	Chromosome *chromosome = CurrentlyInitializingChromosome();
	
	GenomicElementType *genomic_element_type_ptr_0 = ((type_count == 1) ? SLiM_ExtractGenomicElementTypeFromEidosValue_io(genomicElementType_value, 0, &community_, this, "initializeGenomicElement()") : nullptr);					// SPECIES CONSISTENCY CHECK
	GenomicElementType *genomic_element_type_ptr = nullptr;
	slim_position_t start_position = 0, end_position = 0;
	EidosValue_Object *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_GenomicElement_Class))->resize_no_initialize(element_count);
	
	for (int element_index = 0; element_index < element_count; ++element_index)
	{
		genomic_element_type_ptr = ((type_count == 1) ? genomic_element_type_ptr_0 : SLiM_ExtractGenomicElementTypeFromEidosValue_io(genomicElementType_value, element_index, &community_, this, "initializeGenomicElement()"));	// SPECIES CONSISTENCY CHECK
		start_position = SLiMCastToPositionTypeOrRaise(start_value->IntAtIndex_NOCAST(element_index, nullptr));
		end_position = SLiMCastToPositionTypeOrRaise(end_value->IntAtIndex_NOCAST(element_index, nullptr));
		
		if (end_position < start_position)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() end position " << end_position << " is less than start position " << start_position << "." << EidosTerminate();
		
		if (chromosome->extent_immutable_)
		{
			if ((start_position < 0) || (end_position > chromosome->last_position_))
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() genomic element extent lies outside of the extent of the chromosome." << EidosTerminate();
		}
		
		// Check that the new element will not overlap any existing element; if end_position > last_genomic_element_position we are safe.
		// Otherwise, we have to check all previously defined elements.  The use of last_genomic_element_position is an optimization to
		// avoid an O(N) scan with each added element; as long as elements are added in sorted order there is no need to scan.
		if (start_position <= last_genomic_element_position_)
		{
			for (GenomicElement *element : chromosome->GenomicElements())
			{
				if ((element->start_position_ <= end_position) && (element->end_position_ >= start_position))
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElement): initializeGenomicElement() genomic element from start position " << start_position << " to end position " << end_position << " overlaps existing genomic element." << EidosTerminate();
			}
		}
		
		if (end_position > last_genomic_element_position_)
			last_genomic_element_position_ = end_position;
		
		// Create and add the new element
		GenomicElement *new_genomic_element = new GenomicElement(genomic_element_type_ptr, start_position, end_position);
		
		chromosome->GenomicElements().emplace_back(new_genomic_element);
		result_vec->set_object_element_no_check_NORR(new_genomic_element, element_index);
		
		community_.chromosome_changed_ = true;
		num_genomic_element_inits_++;
	}
	
	if (SLiM_verbosity_level >= 1)
	{
		if (ABBREVIATE_DEBUG_INPUT && (num_genomic_element_inits_ > 20) && (num_genomic_element_inits_ != element_count))
		{
			if ((num_genomic_element_inits_ - element_count) <= 20)
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
EidosValue_SP Species::ExecuteContextFunction_initializeGenomicElementType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *mutationTypes_value = p_arguments[1].get();
	EidosValue *proportions_value = p_arguments[2].get();
	EidosValue *mutationMatrix_value = p_arguments[3].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	slim_objectid_t map_identifier = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 'g');
	
	if (community_.GenomicElementTypeWithID(map_identifier))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() genomic element type g" << map_identifier << " already defined." << EidosTerminate();
	
	int mut_type_id_count = mutationTypes_value->Count();
	int proportion_count = proportions_value->Count();
	
	if (mut_type_id_count != proportion_count)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() requires the sizes of mutationTypes and proportions to be equal." << EidosTerminate();
	
	std::vector<MutationType*> mutation_types;
	std::vector<double> mutation_fractions;
	
	for (int mut_type_index = 0; mut_type_index < mut_type_id_count; ++mut_type_index)
	{
		MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationTypes_value, mut_type_index, &community_, this, "initializeGenomicElementType()");		// SPECIES CONSISTENCY CHECK
		double proportion = proportions_value->NumericAtIndex_NOCAST(mut_type_index, nullptr);
		
		if ((proportion < 0) || !std::isfinite(proportion))		// == 0 is allowed but must be fixed before the simulation executes; see InitializeDraws()
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() proportions must be greater than or equal to zero (" << EidosStringForFloat(proportion) << " supplied)." << EidosTerminate();
		
		if (std::find(mutation_types.begin(), mutation_types.end(), mutation_type_ptr) != mutation_types.end())
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() mutation type m" << mutation_type_ptr->mutation_type_id_ << " used more than once." << EidosTerminate();
		
		if (nucleotide_based_ && !mutation_type_ptr->nucleotide_based_)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): in nucleotide-based models, initializeGenomicElementType() requires all mutation types for the genomic element type to be nucleotide-based.  Non-nucleotide-based mutation types may be used in nucleotide-based models, but they cannot be autogenerated by SLiM, and therefore cannot be referenced by a genomic element type." << EidosTerminate();
		
		mutation_types.emplace_back(mutation_type_ptr);
		mutation_fractions.emplace_back(proportion);
		
		// check whether we are using a mutation type that is non-neutral; check and set pure_neutral_
		if ((mutation_type_ptr->dfe_type_ != DFEType::kFixed) || (mutation_type_ptr->dfe_parameters_[0] != 0.0))
		{
			pure_neutral_ = false;
			// the mutation type's all_pure_neutral_DFE_ flag is presumably already set
		}
	}
	
	EidosValueType mm_type = mutationMatrix_value->Type();
	
	if (!nucleotide_based_ && (mm_type != EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() requires mutationMatrix to be NULL in non-nucleotide-based models." << EidosTerminate();
	if (nucleotide_based_ && (mm_type == EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() requires mutationMatrix to be non-NULL in nucleotide-based models." << EidosTerminate();
	
	GenomicElementType *new_genomic_element_type = new GenomicElementType(*this, map_identifier, mutation_types, mutation_fractions);
	if (nucleotide_based_)
		new_genomic_element_type->SetNucleotideMutationMatrix(EidosValue_Float_SP((EidosValue_Float *)(mutationMatrix_value)));
	
	genomic_element_types_.emplace(map_identifier, new_genomic_element_type);
	community_.genomic_element_types_changed_ = true;
	
	// define a new Eidos variable to refer to the new genomic element type
	EidosSymbolTableEntry &symbol_entry = new_genomic_element_type->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGenomicElementType): initializeGenomicElementType() symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
	
	if (SLiM_verbosity_level >= 1)
	{
		if (ABBREVIATE_DEBUG_INPUT && (num_ge_type_inits_ > 99))
		{
			if (num_ge_type_inits_ == 100)
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
				output_stream << (mut_type_index > 0 ? ", " : "") << proportions_value->NumericAtIndex_NOCAST(mut_type_index, nullptr);
			output_stream << ((mut_type_id_count > 1) ? ")" : "");
			
			output_stream << ");" << std::endl;
		}
	}
	
	num_ge_type_inits_++;
	return symbol_entry.second;
}

//	*********************	(object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
//	*********************	(object<MutationType>$)initializeMutationTypeNuc(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
//
EidosValue_SP Species::ExecuteContextFunction_initializeMutationType(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	// Figure out whether the mutation type is nucleotide-based
	bool nucleotide_based = (p_function_name == "initializeMutationTypeNuc");
	
	if (nucleotide_based && !nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationType): initializeMutationTypeNuc() may be only be called in nucleotide-based models." << EidosTerminate();
	
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *dominanceCoeff_value = p_arguments[1].get();
	EidosValue *distributionType_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	slim_objectid_t map_identifier = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 'm');
	double dominance_coeff = dominanceCoeff_value->NumericAtIndex_NOCAST(0, nullptr);
	std::string dfe_type_string = distributionType_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (community_.MutationTypeWithID(map_identifier))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationType): " << p_function_name << "() mutation type m" << map_identifier << " already defined." << EidosTerminate();
	
	// Parse the DFE type and parameters, and do various sanity checks
	DFEType dfe_type;
	std::vector<double> dfe_parameters;
	std::vector<std::string> dfe_strings;
	
	MutationType::ParseDFEParameters(dfe_type_string, p_arguments.data() + 3, (int)p_arguments.size() - 3, &dfe_type, &dfe_parameters, &dfe_strings);
	
#ifdef SLIMGUI
	// each new mutation type gets a unique zero-based index, used by SLiMgui to categorize mutations
	MutationType *new_mutation_type = new MutationType(*this, map_identifier, dominance_coeff, nucleotide_based, dfe_type, dfe_parameters, dfe_strings, num_mutation_type_inits_);
#else
	MutationType *new_mutation_type = new MutationType(*this, map_identifier, dominance_coeff, nucleotide_based, dfe_type, dfe_parameters, dfe_strings);
#endif
	
	mutation_types_.emplace(map_identifier, new_mutation_type);
	community_.mutation_types_changed_ = true;
	
	// keep track of whether we have ever seen a type 's' (scripted) DFE; if so, we switch to a slower case when evolving
	if (dfe_type == DFEType::kScript)
		type_s_dfes_present_ = true;
	
	// define a new Eidos variable to refer to the new mutation type
	EidosSymbolTableEntry &symbol_entry = new_mutation_type->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationType): " << p_function_name << "() symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
	
	if (SLiM_verbosity_level >= 1)
	{
		if (ABBREVIATE_DEBUG_INPUT && (num_mutation_type_inits_ > 99))
		{
			if (num_mutation_type_inits_ == 100)
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
				for (const std::string &dfe_param : dfe_strings)
					output_stream << ", \"" << dfe_param << "\"";
			}
			
			output_stream << ");" << std::endl;
		}
	}
	
	num_mutation_type_inits_++;
	return symbol_entry.second;
}

//	*********************	(void)initializeRecombinationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Species::ExecuteContextFunction_initializeRecombinationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex;
	std::string sex_string = sex_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requested sex '" << sex_string << "' unsupported." << EidosTerminate();
	
	if ((requested_sex != IndividualSex::kUnspecified) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() sex-specific recombination map supplied in non-sexual simulation." << EidosTerminate();
	
	// This function triggers the creation of an implicit chromosome if a chromosome has not already been set up
	if ((num_chromosome_inits_ == 0) && !has_implicit_chromosome_)
		MakeImplicitChromosome(ChromosomeType::kA_DiploidAutosome);
	
	Chromosome *chromosome = CurrentlyInitializingChromosome();
	
	// Make sure specifying a map for that sex is legal, given our current state.  Since single_recombination_map_ has not been set
	// yet, we just look to see whether the chromosome's policy has already been determined or not.
	if (((requested_sex == IndividualSex::kUnspecified) && ((chromosome->recombination_rates_M_.size() != 0) || (chromosome->recombination_rates_F_.size() != 0))) ||
		((requested_sex != IndividualSex::kUnspecified) && (chromosome->recombination_rates_H_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	if (((requested_sex == IndividualSex::kUnspecified) && (num_recrate_inits_ > 0)) || ((requested_sex != IndividualSex::kUnspecified) && (num_recrate_inits_ > 1)))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() may be called only once (or once per sex, with sex-specific recombination maps).  The multiple recombination regions of a recombination map must be set up in a single call to initializeRecombinationRate()." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? chromosome->recombination_end_positions_H_ : 
											   ((requested_sex == IndividualSex::kMale) ? chromosome->recombination_end_positions_M_ : chromosome->recombination_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? chromosome->recombination_rates_H_ : 
								  ((requested_sex == IndividualSex::kMale) ? chromosome->recombination_rates_M_ : chromosome->recombination_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double recombination_rate = rates_value->NumericAtIndex_NOCAST(0, nullptr);
		
		// check values; I thought about requiring a rate of 0.0 for all haploid chromosome types, but maybe
		// the user wants to recombine them sometimes with addRecombinant(), no need to prevent them
		if ((recombination_rate < 0.0) || (recombination_rate > 0.5) || std::isnan(recombination_rate))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires rates to be in [0.0, 0.5] (" << EidosStringForFloat(recombination_rate) << " supplied)." << EidosTerminate();
		
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
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double recombination_rate = rates_value->NumericAtIndex_NOCAST(value_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(value_index, nullptr));
			
			if (value_index > 0)
				if (recombination_end_position <= ends_value->IntAtIndex_NOCAST(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((recombination_rate < 0.0) || (recombination_rate > 0.5) || std::isnan(recombination_rate))
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires rates to be in [0.0, 0.5] (" << EidosStringForFloat(recombination_rate) << " supplied)." << EidosTerminate();
			
			if (chromosome->extent_immutable_)
			{
				if ((recombination_end_position <= 0) || (recombination_end_position > chromosome->last_position_))
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeRecombinationRate): initializeRecombinationRate() requires all end positions to be within the extent of the chromosome." << EidosTerminate();
			}
		}
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double recombination_rate = rates_value->NumericAtIndex_NOCAST(interval_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(interval_index, nullptr));
			
			rates.emplace_back(recombination_rate);
			positions.emplace_back(recombination_end_position);
		}
	}
	
	community_.chromosome_changed_ = true;
	
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
	
	num_recrate_inits_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeGeneConversion(numeric$ nonCrossoverFraction, numeric$ meanLength, numeric$ simpleConversionFraction, [numeric$ bias = 0], [logical$ redrawLengthsOnFailure = F])
//
EidosValue_SP Species::ExecuteContextFunction_initializeGeneConversion(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *nonCrossoverFraction_value = p_arguments[0].get();
	EidosValue *meanLength_value = p_arguments[1].get();
	EidosValue *simpleConversionFraction_value = p_arguments[2].get();
	EidosValue *bias_value = p_arguments[3].get();
	EidosValue *redrawLengthsOnFailure_value = p_arguments[4].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_gene_conv_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() may be called only once." << EidosTerminate();
	
	double non_crossover_fraction = nonCrossoverFraction_value->NumericAtIndex_NOCAST(0, nullptr);
	double gene_conversion_avg_length = meanLength_value->NumericAtIndex_NOCAST(0, nullptr);
	double simple_conversion_fraction = simpleConversionFraction_value->NumericAtIndex_NOCAST(0, nullptr);
	double bias = bias_value->NumericAtIndex_NOCAST(0, nullptr);
	bool redraw_lengths_on_failure = redrawLengthsOnFailure_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if ((non_crossover_fraction < 0.0) || (non_crossover_fraction > 1.0) || std::isnan(non_crossover_fraction))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() nonCrossoverFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(non_crossover_fraction) << " supplied)." << EidosTerminate();
	if ((gene_conversion_avg_length < 0.0) || std::isnan(gene_conversion_avg_length))		// intentionally no upper bound
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() meanLength must be >= 0.0 (" << EidosStringForFloat(gene_conversion_avg_length) << " supplied)." << EidosTerminate();
	if ((simple_conversion_fraction < 0.0) || (simple_conversion_fraction > 1.0) || std::isnan(simple_conversion_fraction))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() simpleConversionFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(simple_conversion_fraction) << " supplied)." << EidosTerminate();
	if ((bias < -1.0) || (bias > 1.0) || std::isnan(bias))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() bias must be between -1.0 and 1.0 inclusive (" << EidosStringForFloat(bias) << " supplied)." << EidosTerminate();
	if ((bias != 0.0) && !nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeGeneConversion): initializeGeneConversion() bias must be 0.0 in non-nucleotide-based models." << EidosTerminate();
	
	// This function triggers the creation of an implicit chromosome if a chromosome has not already been set up
	if ((num_chromosome_inits_ == 0) && !has_implicit_chromosome_)
		MakeImplicitChromosome(ChromosomeType::kA_DiploidAutosome);
	
	Chromosome *chromosome = CurrentlyInitializingChromosome();
	
	chromosome->using_DSB_model_ = true;
	chromosome->non_crossover_fraction_ = non_crossover_fraction;
	chromosome->gene_conversion_avg_length_ = gene_conversion_avg_length;
	chromosome->gene_conversion_inv_half_length_ = 1.0 / (gene_conversion_avg_length / 2.0);
	chromosome->simple_conversion_fraction_ = simple_conversion_fraction;
	chromosome->mismatch_repair_bias_ = bias;
	chromosome->redraw_lengths_on_failure_ = redraw_lengths_on_failure;
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeGeneConversion(" << non_crossover_fraction << ", " << gene_conversion_avg_length << ", " << simple_conversion_fraction << ", " << bias;
		
		if (redraw_lengths_on_failure)
			output_stream << ", T";
		
		output_stream << ");" << std::endl;
	}
	
	num_gene_conv_inits_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeHotspotMap(numeric multipliers, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Species::ExecuteContextFunction_initializeHotspotMap(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	if (!nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() may only be called in nucleotide-based models (use initializeMutationRate() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	EidosValue *multipliers_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	int multipliers_count = multipliers_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex;
	std::string sex_string = sex_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requested sex '" << sex_string << "' unsupported." << EidosTerminate();
	
	if ((requested_sex != IndividualSex::kUnspecified) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() sex-specific hotspot map supplied in non-sexual simulation." << EidosTerminate();
	
	// This function triggers the creation of an implicit chromosome if a chromosome has not already been set up
	if ((num_chromosome_inits_ == 0) && !has_implicit_chromosome_)
		MakeImplicitChromosome(ChromosomeType::kA_DiploidAutosome);
	
	Chromosome *chromosome = CurrentlyInitializingChromosome();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && ((chromosome->hotspot_multipliers_M_.size() != 0) || (chromosome->hotspot_multipliers_F_.size() != 0))) ||
		((requested_sex != IndividualSex::kUnspecified) && (chromosome->hotspot_multipliers_H_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	if (((requested_sex == IndividualSex::kUnspecified) && (num_hotmap_inits_ > 0)) || ((requested_sex != IndividualSex::kUnspecified) && (num_hotmap_inits_ > 1)))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() may be called only once (or once per sex, with sex-specific hotspot maps).  The multiple hotspot regions of a hotspot map must be set up in a single call to initializeHotspotMap()." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? chromosome->hotspot_end_positions_H_ : 
											   ((requested_sex == IndividualSex::kMale) ? chromosome->hotspot_end_positions_M_ : chromosome->hotspot_end_positions_F_));
	std::vector<double> &multipliers = ((requested_sex == IndividualSex::kUnspecified) ? chromosome->hotspot_multipliers_H_ : 
								  ((requested_sex == IndividualSex::kMale) ? chromosome->hotspot_multipliers_M_ : chromosome->hotspot_multipliers_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		if (multipliers_count != 1)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires multipliers to be a singleton if ends is not supplied." << EidosTerminate();
		
		double multiplier = multipliers_value->NumericAtIndex_NOCAST(0, nullptr);
		
		// check values
		if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires multipliers to be >= 0 (" << EidosStringForFloat(multiplier) << " supplied)." << EidosTerminate();
		
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
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires ends and multipliers to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double multiplier = multipliers_value->NumericAtIndex_NOCAST(value_index, nullptr);
			slim_position_t multiplier_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(value_index, nullptr));
			
			if (value_index > 0)
				if (multiplier_end_position <= ends_value->IntAtIndex_NOCAST(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires multipliers to be >= 0 (" << EidosStringForFloat(multiplier) << " supplied)." << EidosTerminate();
			
			if (chromosome->extent_immutable_)
			{
				if ((multiplier_end_position <= 0) || (multiplier_end_position > chromosome->last_position_))
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeHotspotMap): initializeHotspotMap() requires all end positions to be within the extent of the chromosome." << EidosTerminate();
			}
		}
		
		// then adopt them
		multipliers.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double multiplier = multipliers_value->NumericAtIndex_NOCAST(interval_index, nullptr);
			slim_position_t multiplier_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(interval_index, nullptr));
			
			multipliers.emplace_back(multiplier);
			positions.emplace_back(multiplier_end_position);
		}
	}
	
	community_.chromosome_changed_ = true;
	
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
	
	num_hotmap_inits_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeMutationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Species::ExecuteContextFunction_initializeMutationRate(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	if (nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() may not be called in nucleotide-based models (use initializeHotspotMap() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex;
	std::string sex_string = sex_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requested sex '" << sex_string << "' unsupported." << EidosTerminate();
	
	if ((requested_sex != IndividualSex::kUnspecified) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() sex-specific mutation map supplied in non-sexual simulation." << EidosTerminate();
	
	// This function triggers the creation of an implicit chromosome if a chromosome has not already been set up
	if ((num_chromosome_inits_ == 0) && !has_implicit_chromosome_)
		MakeImplicitChromosome(ChromosomeType::kA_DiploidAutosome);
	
	Chromosome *chromosome = CurrentlyInitializingChromosome();
	
	// Make sure specifying a map for that sex is legal, given our current state.  Since single_mutation_map_ has not been set
	// yet, we just look to see whether the chromosome's policy has already been determined or not.
	if (((requested_sex == IndividualSex::kUnspecified) && ((chromosome->mutation_rates_M_.size() != 0) || (chromosome->mutation_rates_F_.size() != 0))) ||
		((requested_sex != IndividualSex::kUnspecified) && (chromosome->mutation_rates_H_.size() != 0)))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	if (((requested_sex == IndividualSex::kUnspecified) && (num_mutrate_inits_ > 0)) || ((requested_sex != IndividualSex::kUnspecified) && (num_mutrate_inits_ > 1)))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() may be called only once (or once per sex, with sex-specific mutation maps).  The multiple mutation regions of a mutation map must be set up in a single call to initializeMutationRate()." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? chromosome->mutation_end_positions_H_ : 
											   ((requested_sex == IndividualSex::kMale) ? chromosome->mutation_end_positions_M_ : chromosome->mutation_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? chromosome->mutation_rates_H_ : 
								  ((requested_sex == IndividualSex::kMale) ? chromosome->mutation_rates_M_ : chromosome->mutation_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double mutation_rate = rates_value->NumericAtIndex_NOCAST(0, nullptr);
		
		// check values
		if ((mutation_rate < 0.0) || (mutation_rate >= 1.0) || !std::isfinite(mutation_rate))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires rates to be >= 0.0 and < 1.0 (" << EidosStringForFloat(mutation_rate) << " supplied)." << EidosTerminate();
		
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
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double mutation_rate = rates_value->NumericAtIndex_NOCAST(value_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(value_index, nullptr));
			
			if (value_index > 0)
				if (mutation_end_position <= ends_value->IntAtIndex_NOCAST(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((mutation_rate < 0.0) || (mutation_rate >= 1.0) || !std::isfinite(mutation_rate))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires rates to be >= 0.0 and < 1.0 (" << EidosStringForFloat(mutation_rate) << " supplied)." << EidosTerminate();
			
			if (chromosome->extent_immutable_)
			{
				if ((mutation_end_position <= 0) || (mutation_end_position > chromosome->last_position_))
					EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeMutationRate): initializeMutationRate() requires all end positions to be within the extent of the chromosome." << EidosTerminate();
			}
		}
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double mutation_rate = rates_value->NumericAtIndex_NOCAST(interval_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex_NOCAST(interval_index, nullptr));
			
			rates.emplace_back(mutation_rate);
			positions.emplace_back(mutation_end_position);
		}
	}
	
	community_.chromosome_changed_ = true;
	
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
	
	num_mutrate_inits_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeSex(Ns$ chromosomeType)
//
EidosValue_SP Species::ExecuteContextFunction_initializeSex(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_arguments, p_interpreter)
	EidosValue *chromosomeType_value = p_arguments[0].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_sex_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSex): initializeSex() may be called only once." << EidosTerminate();
	if (num_chromosome_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSex): initializeSex() must be called before initializeChromosome(), so that initializeChromosome() knows it is in a sexual model." << EidosTerminate();
	
	if (chromosomeType_value->Type() == EidosValueType::kValueNULL)
	{
		// NULL case: we are enabling sex, but not defining an implicit chromosome, and not setting the chromosome type
		// An implicit chromosome is OK in this code path; it has already been assumed to be diploid autosomal, which is fine
		
		if (SLiM_verbosity_level >= 1)
		{
			output_stream << "initializeSex(NULL);" << std::endl;
		}
	}
	else
	{
		// Backward-compatibility case: the user is setting the type of the implicit chromosome with "A", "X", or "Y".
		std::string chromosome_type = chromosomeType_value->StringAtIndex_NOCAST(0, nullptr);
		
		if (chromosome_type.compare(gStr_A) == 0)
		{
			// We want to allow initializeSex() in a no-genetics model; it makes sense to have a sexual but non-genetic species.
			// We allow that only in the "A" case, though; it doesn't make much sense if an "X" or "Y" model is requested.
			// So in this code path we do not make an implicit chromosome; if it is made by somebody else, it will be "A".
		}
		else if ((chromosome_type.compare(gStr_X) == 0) ||
				 (chromosome_type.compare(gStr_Y) == 0))
		{
			// In this "X" / "Y" code path we want to force an implicit chromosome to be defined.
			if (has_implicit_chromosome_)
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSex): initializeSex() with type 'X' or 'Y' must be called before other methods that define an implicit chromosome - initializeAncestralNucleotides(), initializeGeneConversion(), initializeGenomicElement(), initializeHotspotMap(), initializeMutationRate(), and initializeRecombinationRate() - so that the implicit chromosome knows it is a sex chromosome when it is created." << EidosTerminate();
			
			ChromosomeType modeled_chromosome_type;
			
			if (chromosome_type.compare(gStr_X) == 0)
				modeled_chromosome_type = ChromosomeType::kX_XSexChromosome;
			else if (chromosome_type.compare(gStr_Y) == 0)
				modeled_chromosome_type = ChromosomeType::kNullY_YSexChromosomeWithNull;	// not ChromosomeType::kY_YSexChromosome, for backward compatibility
			else
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSex): (internal error) unexpected type." << EidosTerminate();
			
			if ((num_chromosome_inits_ == 0) && !has_implicit_chromosome_)
				MakeImplicitChromosome(modeled_chromosome_type);
		}
		else
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSex): initializeSex() requires a chromosomeType of 'A', 'X', or 'Y' ('" << chromosome_type << "' supplied), or NULL if the chromosome type will be set in initializeChromosome()." << EidosTerminate();
		
		if (SLiM_verbosity_level >= 1)
		{
			output_stream << "initializeSex(\"" << chromosome_type << "\"";
			
			output_stream << ");" << std::endl;
		}
	}
	
	sex_enabled_ = true;
	num_sex_inits_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeSLiMOptions([logical$ keepPedigrees = F], [string$ dimensionality = ""], [string$ periodicity = ""], [logical$ doMutationRunExperiments = T], [logical$ preventIncidentalSelfing = F], [logical$ nucleotideBased = F], [logical$ randomizeCallbacks = T])
//
EidosValue_SP Species::ExecuteContextFunction_initializeSLiMOptions(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_interpreter)
	EidosValue *arg_keepPedigrees_value = p_arguments[0].get();
	EidosValue *arg_dimensionality_value = p_arguments[1].get();
	EidosValue *arg_periodicity_value = p_arguments[2].get();
	EidosValue *arg_doMutationRunExperiments_value = p_arguments[3].get();
	EidosValue *arg_preventIncidentalSelfing_value = p_arguments[4].get();
	EidosValue *arg_nucleotideBased_value = p_arguments[5].get();
	EidosValue *arg_randomizeCallbacks_value = p_arguments[6].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_slimoptions_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSLiMOptions): initializeSLiMOptions() may be called only once." << EidosTerminate();
	
	if (num_chromosome_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSLiMOptions): initializeSLiMOptions() must be called before initializeChromosome(), so that initializeChromosome() has the model configuration information it needs to set up the chromosome." << EidosTerminate();
	
	// see also Species::HasDoneAnyInitialization() for the check used by initializeModelType()
	// we have no order-dependency with initializeSpecies()
	if ((num_mutation_type_inits_ > 0) || (num_mutrate_inits_ > 0) || (num_ge_type_inits_ > 0) || (num_genomic_element_inits_ > 0) || (num_recrate_inits_ > 0) || (num_gene_conv_inits_ > 0) || (num_sex_inits_ > 0) || (num_treeseq_inits_ > 0) || (num_ancseq_inits_ > 0) || (num_hotmap_inits_ > 0) || has_implicit_chromosome_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSLiMOptions): initializeSLiMOptions() must be called before all other species-specific initialization functions." << EidosTerminate();
	
	{
		// [logical$ keepPedigrees = F]
		bool keep_pedigrees = arg_keepPedigrees_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		if (keep_pedigrees)
		{
			// pedigree recording can always be turned on by the user
			pedigrees_enabled_ = true;
			pedigrees_enabled_by_user_ = true;
		}
		else	// !keep_pedigrees
		{
			if (pedigrees_enabled_by_SLiM_)
			{
				// if pedigrees were forced on by tree-seq recording or SLiMgui, they stay on, but we remember that the user wanted them off
				pedigrees_enabled_by_user_ = false;
			}
			else
			{
				// otherwise, the user can turn them off if so desired
				pedigrees_enabled_ = false;
				pedigrees_enabled_by_user_ = false;
			}
		}
	}
	
	{
		// [string$ dimensionality = ""]
		std::string space = arg_dimensionality_value->StringAtIndex_NOCAST(0, nullptr);
		
		if (space.length() != 0)
		{
			if (space == "x")
				spatial_dimensionality_ = 1;
			else if (space == "xy")
				spatial_dimensionality_ = 2;
			else if (space == "xyz")
				spatial_dimensionality_ = 3;
			else
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), legal non-empty values for parameter dimensionality are only 'x', 'xy', and 'xyz'." << EidosTerminate();
		}
	}
	
	{
		// [string$ periodicity = ""]
		std::string periodicity = arg_periodicity_value->StringAtIndex_NOCAST(0, nullptr);
		
		if (periodicity.length() != 0)
		{
			if (spatial_dimensionality_ == 0)
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), parameter periodicity may not be set in non-spatial simulations." << EidosTerminate();
			
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
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), legal non-empty values for parameter periodicity are only 'x', 'y', 'z', 'xy', 'xz', 'yz', and 'xyz'." << EidosTerminate();
			
			if ((periodic_y_ && (spatial_dimensionality_ < 2)) || (periodic_z_ && (spatial_dimensionality_ < 3)))
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSLiMOptions): in initializeSLiMOptions(), parameter periodicity cannot utilize spatial dimensions beyond those set by the dimensionality parameter of initializeSLiMOptions()." << EidosTerminate();
		}
	}
	
	{
		// [logical$ doMutationRunExperiments = T]
		// note this parameter position used to be [integer$ mutationRuns = 0] instead!
		bool do_mutrun_experiments = arg_doMutationRunExperiments_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		do_mutrun_experiments_ = do_mutrun_experiments;
	}
	
	{
		// [logical$ preventIncidentalSelfing = F]
		bool prevent_selfing = arg_preventIncidentalSelfing_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		prevent_incidental_selfing_ = prevent_selfing;
	}
	
	{
		// [logical$ nucleotideBased = F]
		bool nucleotide_based = arg_nucleotideBased_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		nucleotide_based_ = nucleotide_based;
	}
	
	{
		// [logical$ randomizeCallbacks = T]
		bool randomize_callbacks = arg_randomizeCallbacks_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		shuffle_buf_is_enabled_ = randomize_callbacks;
	}
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeSLiMOptions(";
		
		bool previous_params = false;
		
		if (pedigrees_enabled_by_user_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "keepPedigrees = " << (pedigrees_enabled_by_user_ ? "T" : "F");
			previous_params = true;
		}
		
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
		}
		
		if (!shuffle_buf_is_enabled_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "randomizeCallbacks = " << (shuffle_buf_is_enabled_ ? "T" : "F");
			previous_params = true;
			(void)previous_params;	// dead store above is deliberate
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_slimoptions_inits_++;
	
	return gStaticEidosValueVOID;
}

//	*********************	(void)initializeSpecies([integer$ tickModulo = 1], [integer$ tickPhase = 1], [string$ avatar = ""], [string$ color = ""])
//
EidosValue_SP Species::ExecuteContextFunction_initializeSpecies(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_interpreter)
	EidosValue *arg_tickModulo_value = p_arguments[0].get();
	EidosValue *arg_tickPhase_value = p_arguments[1].get();
	EidosValue *arg_avatar_value = p_arguments[2].get();
	EidosValue *arg_color_value = p_arguments[3].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	// BCH 27 March 2022: This is not actually necessary, but it seems best to draw a sharp line between explicit-species models
	// and implied-species (single-species) models, to avoid confusion.  We do the same for 'ticks' and 'species' specifications
	// on events and callbacks.  If you want to do species-related stuff, declare your species.
	if (!community_.is_explicit_species_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSpecies): initializeSpecies() may only be called if species have been explicitly declared, with a 'species <name>' specifier preceding an initialize() callback." << EidosTerminate();
	
	if (num_species_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSpecies): initializeSpecies() may be called only once per species." << EidosTerminate();
	
	if (num_chromosome_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSpecies): initializeSpecies() must be called before initializeChromosome(), so that initializeChromosome() has the model configuration information it needs to set up the chromosome." << EidosTerminate();
	
	int64_t tickModulo = arg_tickModulo_value->IntAtIndex_NOCAST(0, nullptr);
	
	if ((tickModulo < 1) || (tickModulo >= SLIM_MAX_TICK))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSpecies): initializeSpecies() requires a tickModulo value >= 1." << EidosTerminate();
	
	tick_modulo_ = (slim_tick_t)tickModulo;
	
	int64_t tickPhase = arg_tickPhase_value->IntAtIndex_NOCAST(0, nullptr);
	
	if ((tickPhase < 1) || (tickModulo >= SLIM_MAX_TICK))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeSpecies): initializeSpecies() requires a tickPhase value >= 1." << EidosTerminate();
	
	tick_phase_ = (slim_tick_t)tickPhase;
	
	avatar_ = arg_avatar_value->StringAtIndex_NOCAST(0, nullptr);
	
	color_ = arg_color_value->StringAtIndex_NOCAST(0, nullptr);
	if (!color_.empty())
		Eidos_GetColorComponents(color_, &color_red_, &color_green_, &color_blue_);
	
	if (SLiM_verbosity_level >= 1)
	{
		output_stream << "initializeSpecies(";
		
		bool previous_params = false;
		
		if (tickModulo != 1)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "tickModulo = " << tickModulo;
			previous_params = true;
		}
		
		if (tickPhase != 1)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "tickPhase = " << tickPhase;
			previous_params = true;
		}
		
		if (avatar_.length() > 0)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "avatar = \"" << avatar_ << "\"";
			previous_params = true;
		}
		
		if (color_.length() > 0)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "color = \"" << color_ << "\"";
			previous_params = true;
			(void)previous_params;	// dead store above is deliberate
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_species_inits_++;
	
	return gStaticEidosValueVOID;
}

// TREE SEQUENCE RECORDING
//	*********************	(void)initializeTreeSeq([logical$ recordMutations = T], [Nif$ simplificationRatio = NULL], [Ni$ simplificationInterval = NULL], [logical$ checkCoalescence = F], [logical$ runCrosschecks = F], [logical$ retainCoalescentOnly = T], [Ns$ timeUnit = NULL])
//
EidosValue_SP Species::ExecuteContextFunction_initializeTreeSeq(const std::string &p_function_name, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_function_name, p_interpreter)
	EidosValue *arg_recordMutations_value = p_arguments[0].get();
	EidosValue *arg_simplificationRatio_value = p_arguments[1].get();
	EidosValue *arg_simplificationInterval_value = p_arguments[2].get();
	EidosValue *arg_checkCoalescence_value = p_arguments[3].get();
	EidosValue *arg_runCrosschecks_value = p_arguments[4].get();
	EidosValue *arg_retainCoalescentOnly_value = p_arguments[5].get();
	EidosValue *arg_timeUnit_value = p_arguments[6].get();
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	if (num_treeseq_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() may be called only once." << EidosTerminate();
	
	if (num_chromosome_inits_ > 0)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() must be called before initializeChromosome(), so that initializeChromosome() has the model configuration information it needs to set up the chromosome." << EidosTerminate();
	
	// NOTE: the TSXC_Enable() method also sets up tree-seq recording by setting these sorts of flags;
	// if the code here changes, that method should probably be updated too.
	
	recording_tree_ = true;
	recording_mutations_ = arg_recordMutations_value->LogicalAtIndex_NOCAST(0, nullptr);
	running_coalescence_checks_ = arg_checkCoalescence_value->LogicalAtIndex_NOCAST(0, nullptr);
	running_treeseq_crosschecks_ = arg_runCrosschecks_value->LogicalAtIndex_NOCAST(0, nullptr);
	retain_coalescent_only_ = arg_retainCoalescentOnly_value->LogicalAtIndex_NOCAST(0, nullptr);
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
		simplification_ratio_ = arg_simplificationRatio_value->NumericAtIndex_NOCAST(0, nullptr);
		simplification_interval_ = -1;
		
		if (std::isnan(simplification_ratio_) || (simplification_ratio_ < 0))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() requires simplificationRatio to be >= 0." << EidosTerminate();
		
		// Choose an initial auto-simplification interval
		if (arg_simplificationInterval_value->Type() != EidosValueType::kValueNULL)
		{
			// Both ratio and interval are non-NULL; the interval is thus interpreted as the *initial* interval
			simplify_interval_ = arg_simplificationInterval_value->IntAtIndex_NOCAST(0, nullptr);
			
			if (simplify_interval_ <= 0)
				EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() requires simplificationInterval to be > 0." << EidosTerminate();
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
		simplification_interval_ = arg_simplificationInterval_value->IntAtIndex_NOCAST(0, nullptr);
		
		if (simplification_interval_ <= 0)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() requires simplificationInterval to be > 0." << EidosTerminate();
	}
	
	// Pedigree recording is turned on as a side effect of tree sequence recording, since we need to
	// have unique identifiers for every individual; pedigree recording does that for us
	pedigrees_enabled_ = true;
	pedigrees_enabled_by_SLiM_ = true;
	
	// Get the time units if set, or set the default time unit as appropriate
	if (arg_timeUnit_value->Type() == EidosValueType::kValueNULL)
	{
		// In SLiM 3.7 we set the time unit to "generations" for WF models since generations are non-overlapping
		// there, and to "ticks" in nonWF models.  In SLiM 4 we set it to "ticks" in all cases, since with the
		// multispecies changes different WF species may run on different timescales.  A tick is a tick.  The user
		// can set this otherwise if they want to; we should not try to second-guess what is going on.
		community_.treeseq_time_unit_ = "ticks";
	}
	else
	{
		community_.treeseq_time_unit_ = arg_timeUnit_value->StringAtIndex_NOCAST(0, nullptr);
		
		if ((community_.treeseq_time_unit_.length() == 0) || (community_.treeseq_time_unit_.find('"') != std::string::npos) || (community_.treeseq_time_unit_.find('\'') != std::string::npos))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteContextFunction_initializeTreeSeq): initializeTreeSeq() requires the timeUnit to be non-zero length, and it may not contain a quote character." << EidosTerminate();
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
			output_stream << "simplificationInterval = " << arg_simplificationInterval_value->IntAtIndex_NOCAST(0, nullptr);
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
		}
		
		if (!retain_coalescent_only_)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "retainCoalescentOnly = " << (retain_coalescent_only_ ? "T" : "F");
			previous_params = true;
		}
		
		if (arg_timeUnit_value->Type() != EidosValueType::kValueNULL)
		{
			if (previous_params) output_stream << ", ";
			output_stream << "timeUnit = '" << community_.treeseq_time_unit_ << "'";	// assumes a simple string with no quotes
			previous_params = true;
			(void)previous_params;	// dead store above is deliberate
		}
		
		output_stream << ");" << std::endl;
	}
	
	num_treeseq_inits_++;
	
	return gStaticEidosValueVOID;
}


const EidosClass *Species::Class(void) const
{
	return gSLiM_Species_Class;
}

void Species::Print(std::ostream &p_ostream) const
{
	// Show the avatar in multispecies models (or any explicit species model)
	if (community_.is_explicit_species_)
		p_ostream << Class()->ClassNameForDisplay() << "<" << species_id_ << ":" << avatar_ << ">";
	else
		p_ostream << Class()->ClassNameForDisplay() << "<" << species_id_ << ">";
}

EidosValue_SP Species::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_avatar:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(avatar_));
		}
		case gID_chromosome:
		{
			if (chromosomes_.size() != 1)
				EIDOS_TERMINATION << "ERROR (Species::GetProperty): property chromosome may only be accessed on a species that has exactly one chromosome; in all other cases the chromosomes property must be used, since it can return multiple chromosomes (or none)." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(chromosomes_[0], gSLiM_Chromosome_Class));
		}
		case gID_chromosomes:
		{
			EidosValue_Object *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Chromosome_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (Chromosome *chromosome : chromosomes_)
				vec->push_object_element_RR(chromosome);
			
			return result_SP;
		}
		case gEidosID_color:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(color_));
		}
		case gID_dimensionality:
		{
			static EidosValue_SP static_dimensionality_string_x;
			static EidosValue_SP static_dimensionality_string_xy;
			static EidosValue_SP static_dimensionality_string_xyz;
			
#pragma omp critical (GetProperty_dimensionality_cache)
			{
				if (!static_dimensionality_string_x)
				{
					static_dimensionality_string_x = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gEidosStr_x));
					static_dimensionality_string_xy = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("xy"));
					static_dimensionality_string_xyz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("xyz"));
				}
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
		case gID_id:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(species_id_));
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
			
#pragma omp critical (GetProperty_periodicity_cache)
			{
				if (!static_periodicity_string_x)
				{
					static_periodicity_string_x = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gEidosStr_x));
					static_periodicity_string_y = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gEidosStr_y));
					static_periodicity_string_z = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gEidosStr_z));
					static_periodicity_string_xy = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("xy"));
					static_periodicity_string_xz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("xz"));
					static_periodicity_string_yz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("yz"));
					static_periodicity_string_xyz = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("xyz"));
				}
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
			EidosValue_Object *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_GenomicElementType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto ge_type : genomic_element_types_)
				vec->push_object_element_NORR(ge_type.second);
			
			return result_SP;
		}
		case gID_mutations:
		{
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			int registry_size;
			const MutationIndex *registry = population_.MutationRegistry(&registry_size);
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->resize_no_initialize_RR(registry_size);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + registry[registry_index], registry_index);
			
			return result_SP;
		}
		case gID_mutationTypes:
		{
			EidosValue_Object *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_MutationType_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto mutation_type : mutation_types_)
				vec->push_object_element_NORR(mutation_type.second);
			
			return result_SP;
		}
		case gID_name:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(name_));
		}
		case gID_nucleotideBased:
		{
			return (nucleotide_based_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_scriptBlocks:
		{
			EidosValue_Object *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_SLiMEidosBlock_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			const std::vector<SLiMEidosBlock*> &script_blocks = community_.AllScriptBlocksForSpecies(this);		// this will only be species-specific callbacks
			
			for (auto script_block : script_blocks)
				vec->push_object_element_NORR(script_block);
			
			return result_SP;
		}
		case gID_sexChromosomes:
		{
			EidosValue_Object *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Chromosome_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (Chromosome *chromosome : chromosomes_)
				if (chromosome->IsSexChromosome())
					vec->push_object_element_RR(chromosome);
			
			return result_SP;
		}
		case gID_sexEnabled:
			return (sex_enabled_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_subpopulations:
		{
			EidosValue_Object *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Subpopulation_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto pop : population_.subpops_)
				vec->push_object_element_NORR(pop.second);
			
			return result_SP;
		}
		case gID_substitutions:
		{
			std::vector<Substitution*> &substitutions = population_.substitutions_;
			int substitution_count = (int)substitutions.size();
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Substitution_Class))->resize_no_initialize_RR(substitution_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (int sub_index = 0; sub_index < substitution_count; ++sub_index)
				vec->set_object_element_no_check_no_previous_RR(substitutions[sub_index], sub_index);
			
			return result_SP;
		}
			
			// variables
		case gID_description:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(description_));
		}
		case gID_cycle:
		{
			if (cached_value_cycle_ && (cached_value_cycle_->IntData()[0] != cycle_))
				cached_value_cycle_.reset();
			if (!cached_value_cycle_)
				cached_value_cycle_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(cycle_));
			return cached_value_cycle_;
		}
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Species::GetProperty): property tag accessed on simulation object before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

void Species::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_description:
		{
			std::string description = p_value.StringAtIndex_NOCAST(0, nullptr);
			
			// there are no restrictions on descriptions at all
			
			description_ = description;
			return;
		}
		case gID_cycle:
		{
			int64_t value = p_value.IntAtIndex_NOCAST(0, nullptr);
			slim_tick_t old_cycle = cycle_;
			slim_tick_t new_cycle = SLiMCastToTickTypeOrRaise(value);
			
			if (new_cycle != old_cycle)
				SetCycle(new_cycle);
			return;
		}
			
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return super::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP Species::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
			// WF only:
		case gID_addSubpopSplit:					return ExecuteMethod_addSubpopSplit(p_method_id, p_arguments, p_interpreter);
			
		case gID_addPatternForClone:				return ExecuteMethod_addPatternForClone(p_method_id, p_arguments, p_interpreter);
		case gID_addPatternForCross:				return ExecuteMethod_addPatternForCross(p_method_id, p_arguments, p_interpreter);
		case gID_addPatternForNull:					return ExecuteMethod_addPatternForNull(p_method_id, p_arguments, p_interpreter);
		case gID_addPatternForRecombinant:			return ExecuteMethod_addPatternForRecombinant(p_method_id, p_arguments, p_interpreter);
		case gID_addSubpop:							return ExecuteMethod_addSubpop(p_method_id, p_arguments, p_interpreter);
		case gID_chromosomesOfType:					return ExecuteMethod_chromosomesOfType(p_method_id, p_arguments, p_interpreter);
		case gID_chromosomesWithIDs:				return ExecuteMethod_chromosomesWithIDs(p_method_id, p_arguments, p_interpreter);
		case gID_chromosomesWithSymbols:			return ExecuteMethod_chromosomesWithSymbols(p_method_id, p_arguments, p_interpreter);
		case gID_individualsWithPedigreeIDs:		return ExecuteMethod_individualsWithPedigreeIDs(p_method_id, p_arguments, p_interpreter);
		case gID_killIndividuals:					return ExecuteMethod_killIndividuals(p_method_id, p_arguments, p_interpreter);
		case gID_mutationFrequencies:
		case gID_mutationCounts:					return ExecuteMethod_mutationFreqsCounts(p_method_id, p_arguments, p_interpreter);
		case gID_mutationsOfType:					return ExecuteMethod_mutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_countOfMutationsOfType:			return ExecuteMethod_countOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_outputFixedMutations:				return ExecuteMethod_outputFixedMutations(p_method_id, p_arguments, p_interpreter);
		case gID_outputFull:						return ExecuteMethod_outputFull(p_method_id, p_arguments, p_interpreter);
		case gID_outputMutations:					return ExecuteMethod_outputMutations(p_method_id, p_arguments, p_interpreter);
		case gID_readFromPopulationFile:			return ExecuteMethod_readFromPopulationFile(p_method_id, p_arguments, p_interpreter);
		case gID_recalculateFitness:				return ExecuteMethod_recalculateFitness(p_method_id, p_arguments, p_interpreter);
		case gID_registerFitnessEffectCallback:		return ExecuteMethod_registerFitnessEffectCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerMateChoiceCallback:
		case gID_registerModifyChildCallback:
		case gID_registerRecombinationCallback:
		case gID_registerSurvivalCallback:			return ExecuteMethod_registerMateModifyRecSurvCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerMutationCallback:			return ExecuteMethod_registerMutationCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerMutationEffectCallback:	return ExecuteMethod_registerMutationEffectCallback(p_method_id, p_arguments, p_interpreter);
		case gID_registerReproductionCallback:		return ExecuteMethod_registerReproductionCallback(p_method_id, p_arguments, p_interpreter);
		case gID_simulationFinished:				return ExecuteMethod_simulationFinished(p_method_id, p_arguments, p_interpreter);
		case gID_skipTick:							return ExecuteMethod_skipTick(p_method_id, p_arguments, p_interpreter);
		case gID_subsetMutations:					return ExecuteMethod_subsetMutations(p_method_id, p_arguments, p_interpreter);
		case gID_substitutionsOfType:				return ExecuteMethod_substitutionsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqCoalesced:					return ExecuteMethod_treeSeqCoalesced(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqSimplify:					return ExecuteMethod_treeSeqSimplify(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqRememberIndividuals:		return ExecuteMethod_treeSeqRememberIndividuals(p_method_id, p_arguments, p_interpreter);
		case gID_treeSeqOutput:						return ExecuteMethod_treeSeqOutput(p_method_id, p_arguments, p_interpreter);
		case gID__debug:							return ExecuteMethod__debug(p_method_id, p_arguments, p_interpreter);
		default:									return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (object<Dictionary>$)addPatternForClone(iso<Chromosome>$ chromosome, No<Dictionary>$ pattern, object<Individual>$ parent, [Ns$ sex = NULL])
//
EidosValue_SP Species::ExecuteMethod_addPatternForClone(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *chromosome_value = p_arguments[0].get();
	EidosValue *pattern_value = p_arguments[1].get();
	EidosValue *parent_value = p_arguments[2].get();
	EidosValue *sex_value = p_arguments[3].get();
	
	// Get the focal chromosome; NULL is not allowed by signature
	Chromosome *chromosome = GetChromosomeFromEidosValue(chromosome_value);
	
	// Get or construct the pattern dictionary; result_SP keeps a retain on it
	EidosDictionaryUnretained *pattern;
	EidosValue_SP result_SP(nullptr);
	bool pattern_uses_integer_keys;
	
	if (pattern_value->Type() == EidosValueType::kValueNULL)
	{
		EidosDictionaryRetained *pattern_retained = new EidosDictionaryRetained();
		pattern = pattern_retained;
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(pattern, gEidosDictionaryRetained_Class));
		pattern_retained->Release();	// retained by result_SP
		pattern_uses_integer_keys = true;
	}
	else
	{
		pattern = (EidosDictionaryUnretained *)pattern_value->ObjectData()[0];
		result_SP = p_arguments[1];
		pattern_uses_integer_keys = pattern->KeysAreIntegers();
	}
	
	// Get the offspring sex
	IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() == EidosValueType::kValueString)
	{
		const std::string &sex_string = sex_value->StringData()[0];
		
		if (sex_string.compare("M") == 0)
			sex = IndividualSex::kMale;
		else if (sex_string.compare("F") == 0)
			sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForClone): addPatternForClone() requires sex to be 'M' or 'F', or NULL." << EidosTerminate();
	}
	
	// make a new inheritance dictionary and add it to pattern
	EidosDictionaryRetained *inheritance = new EidosDictionaryRetained();
	EidosValue_SP inheritance_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(inheritance, gEidosDictionaryRetained_Class));
	inheritance->Release();
	
	if (pattern_uses_integer_keys)
		pattern->SetKeyValue_IntegerKeys(chromosome->ID(), inheritance_SP);
	else
		pattern->SetKeyValue_StringKeys(chromosome->Symbol(), inheritance_SP);
	
	//
	//	the above code is shared with the other addPatternFor...() methods; the remainder of the code is not
	//
	
	// Get the parent for cloning and get info about it
	Individual *parent = (Individual *)parent_value->ObjectData()[0];
	
	// SPECIES CONSISTENCY CHECK
	if (&parent->subpopulation_->species_ != this)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForClone): addPatternForClone() requires that parent belong to the target species." << EidosTerminate();
	
	// get the inheritance pattern; there are at most two strands involved, and no recombination
	Haplosome *strand1 = nullptr, *strand3 = nullptr;
	
	InferInheritanceForClone(chromosome, parent, sex, &strand1, &strand3, "addPatternForClone()");
	
	// set the inheritance pattern into the dictionary
	if (strand1)	inheritance->SetKeyValue_StringKeys(gStr_strand1, strand1->CachedEidosValue());
	if (strand3)	inheritance->SetKeyValue_StringKeys(gStr_strand3, strand3->CachedEidosValue());
	
	pattern->ContentsChanged("Dictionary()");
	return result_SP;
}

//	*********************	– (object<Dictionary>$)addPatternForCross(iso<Chromosome>$ chromosome, No<Dictionary>$ pattern, object<Individual>$ parent1, object<Individual>$ parent2, [Ns$ sex = NULL])
//
EidosValue_SP Species::ExecuteMethod_addPatternForCross(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *chromosome_value = p_arguments[0].get();
	EidosValue *pattern_value = p_arguments[1].get();
	EidosValue *parent1_value = p_arguments[2].get();
	EidosValue *parent2_value = p_arguments[3].get();
	EidosValue *sex_value = p_arguments[4].get();
	
	// Get the focal chromosome; NULL is not allowed by signature
	Chromosome *chromosome = GetChromosomeFromEidosValue(chromosome_value);
	
	// Get or construct the pattern dictionary; result_SP keeps a retain on it
	EidosDictionaryUnretained *pattern;
	EidosValue_SP result_SP(nullptr);
	bool pattern_uses_integer_keys;
	
	if (pattern_value->Type() == EidosValueType::kValueNULL)
	{
		EidosDictionaryRetained *pattern_retained = new EidosDictionaryRetained();
		pattern = pattern_retained;
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(pattern, gEidosDictionaryRetained_Class));
		pattern_retained->Release();	// retained by result_SP
		pattern_uses_integer_keys = true;
	}
	else
	{
		pattern = (EidosDictionaryUnretained *)pattern_value->ObjectData()[0];
		result_SP = p_arguments[1];
		pattern_uses_integer_keys = pattern->KeysAreIntegers();
	}
	
	// Get the offspring sex
	IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() == EidosValueType::kValueString)
	{
		const std::string &sex_string = sex_value->StringData()[0];
		
		if (sex_string.compare("M") == 0)
			sex = IndividualSex::kMale;
		else if (sex_string.compare("F") == 0)
			sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForCross): addPatternForCross() requires sex to be 'M' or 'F', or NULL." << EidosTerminate();
	}
	
	// make a new inheritance dictionary and add it to pattern
	EidosDictionaryRetained *inheritance = new EidosDictionaryRetained();
	EidosValue_SP inheritance_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(inheritance, gEidosDictionaryRetained_Class));
	inheritance->Release();
	
	if (pattern_uses_integer_keys)
		pattern->SetKeyValue_IntegerKeys(chromosome->ID(), inheritance_SP);
	else
		pattern->SetKeyValue_StringKeys(chromosome->Symbol(), inheritance_SP);
	
	//
	//	the above code is shared with the other addPatternFor...() methods; the remainder of the code is not
	//
	
	// Get the parents for crossing and validate them
	Individual *parent1 = (Individual *)parent1_value->ObjectData()[0];
	Individual *parent2 = (Individual *)parent2_value->ObjectData()[0];
	
	// SPECIES CONSISTENCY CHECK
	if ((&parent1->subpopulation_->species_ != this) || (&parent2->subpopulation_->species_ != this))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForCross): addPatternForCross() requires that parent1 and parent2 belong to the target species." << EidosTerminate();
	
	// get the inheritance pattern; there are at most two strands involved, and no recombination
	Haplosome *strand1 = nullptr, *strand2 = nullptr, *strand3 = nullptr, *strand4 = nullptr;
	
	InferInheritanceForCross(chromosome, parent1, parent2, sex, &strand1, &strand2, &strand3, &strand4, "addPatternForCross()");
	
	// set the inheritance pattern into the dictionary
	if (strand1)	inheritance->SetKeyValue_StringKeys(gStr_strand1, strand1->CachedEidosValue());
	if (strand2)	inheritance->SetKeyValue_StringKeys(gStr_strand2, strand2->CachedEidosValue());
	if (strand3)	inheritance->SetKeyValue_StringKeys(gStr_strand3, strand3->CachedEidosValue());
	if (strand4)	inheritance->SetKeyValue_StringKeys(gStr_strand4, strand4->CachedEidosValue());
	
	pattern->ContentsChanged("Dictionary()");
	return result_SP;
}

//	*********************	– (object<Dictionary>$)addPatternForNull(iso<Chromosome>$ chromosome, No<Dictionary>$ pattern, [Ns$ sex = NULL])
//
EidosValue_SP Species::ExecuteMethod_addPatternForNull(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *chromosome_value = p_arguments[0].get();
	EidosValue *pattern_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	
	// Get the focal chromosome; NULL is not allowed by signature
	Chromosome *chromosome = GetChromosomeFromEidosValue(chromosome_value);
	ChromosomeType chromosome_type = chromosome->Type();
	
	// Get or construct the pattern dictionary; result_SP keeps a retain on it
	EidosDictionaryUnretained *pattern;
	EidosValue_SP result_SP(nullptr);
	bool pattern_uses_integer_keys;
	
	if (pattern_value->Type() == EidosValueType::kValueNULL)
	{
		EidosDictionaryRetained *pattern_retained = new EidosDictionaryRetained();
		pattern = pattern_retained;
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(pattern, gEidosDictionaryRetained_Class));
		pattern_retained->Release();	// retained by result_SP
		pattern_uses_integer_keys = true;
	}
	else
	{
		pattern = (EidosDictionaryUnretained *)pattern_value->ObjectData()[0];
		result_SP = p_arguments[1];
		pattern_uses_integer_keys = pattern->KeysAreIntegers();
	}
	
	// Get the offspring sex
	IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() == EidosValueType::kValueString)
	{
		const std::string &sex_string = sex_value->StringData()[0];
		
		if (sex_string.compare("M") == 0)
			sex = IndividualSex::kMale;
		else if (sex_string.compare("F") == 0)
			sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForNull): addPatternForNull() requires sex to be 'M' or 'F', or NULL." << EidosTerminate();
	}
	
	// make a new inheritance dictionary and add it to pattern
	EidosDictionaryRetained *inheritance = new EidosDictionaryRetained();
	EidosValue_SP inheritance_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(inheritance, gEidosDictionaryRetained_Class));
	inheritance->Release();
	
	if (pattern_uses_integer_keys)
		pattern->SetKeyValue_IntegerKeys(chromosome->ID(), inheritance_SP);
	else
		pattern->SetKeyValue_StringKeys(chromosome->Symbol(), inheritance_SP);
	
	//
	//	the above code is shared with the other addPatternFor...() methods; the remainder of the code is not
	//
	
	if ((chromosome_type == ChromosomeType::kX_XSexChromosome) ||
		(chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
		(chromosome_type == ChromosomeType::kHF_HaploidFemaleInherited) ||
		(chromosome_type == ChromosomeType::kHM_HaploidMaleInherited) ||
		(chromosome_type == ChromosomeType::kHNull_HaploidAutosomeWithNull))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForNull): addPatternForNull() cannot be used with chromosome type '" << chromosome_type << "', since all individuals must possess at least one non-null haplosomes for that chromosome type.  For greater flexibility, use chromosome type 'A' or 'H'." << EidosTerminate();
	
	// check that the offspring sex is compatible with having all null haplosomes for this chromosome
	if ((sex == IndividualSex::kUnspecified) || (sex == IndividualSex::kFemale))
		if ((chromosome_type == ChromosomeType::kW_WSexChromosome) ||
			(chromosome_type == ChromosomeType::kFL_HaploidFemaleLine))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForNull): addPatternForNull() requires sex to be 'M' for chromosome type '', since only males can have all null haplosomes for that chromosome type." << EidosTerminate();
	if ((sex == IndividualSex::kUnspecified) || (sex == IndividualSex::kMale))
		if ((chromosome_type == ChromosomeType::kY_YSexChromosome) ||
			(chromosome_type == ChromosomeType::kML_HaploidMaleLine) ||
			(chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForNull): addPatternForNull() requires sex to be 'F' for chromosome type '', since only females can have all null haplosomes for that chromosome type." << EidosTerminate();
	
	// set the inheritance pattern into the dictionary; there no code code here because the offspring
	// inherits nothing, so the inheritance dictionary should just be an empty dictionary, NULL for all
	
	pattern->ContentsChanged("Dictionary()");
	return result_SP;
}

//	*********************	– (object<Dictionary>$)addPatternForRecombinant(iso<Chromosome>$ chromosome, No<Dictionary>$ pattern, No<Haplosome>$ strand1, No<Haplosome>$ strand2, Ni breaks1, No<Haplosome>$ strand3, No<Haplosome>$ strand4, Ni breaks2, [Ns$ sex = NULL], [logical$ randomizeStrands = T])
//
EidosValue_SP Species::ExecuteMethod_addPatternForRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *chromosome_value = p_arguments[0].get();
	EidosValue *pattern_value = p_arguments[1].get();
	EidosValue *strand1_value = p_arguments[2].get();
	EidosValue *strand2_value = p_arguments[3].get();
	EidosValue *breaks1_value = p_arguments[4].get();
	EidosValue *strand3_value = p_arguments[5].get();
	EidosValue *strand4_value = p_arguments[6].get();
	EidosValue *breaks2_value = p_arguments[7].get();
	EidosValue *sex_value = p_arguments[8].get();
	EidosValue *randomizeStrands_value = p_arguments[9].get();
	
	// Get the focal chromosome; NULL is not allowed by signature
	Chromosome *chromosome = GetChromosomeFromEidosValue(chromosome_value);
	ChromosomeType chromosome_type = chromosome->Type();
	slim_chromosome_index_t chromosome_index = chromosome->Index();
	
	// Get or construct the pattern dictionary; result_SP keeps a retain on it
	EidosDictionaryUnretained *pattern;
	EidosValue_SP result_SP(nullptr);
	bool pattern_uses_integer_keys;
	
	if (pattern_value->Type() == EidosValueType::kValueNULL)
	{
		EidosDictionaryRetained *pattern_retained = new EidosDictionaryRetained();
		pattern = pattern_retained;
		
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(pattern, gEidosDictionaryRetained_Class));
		pattern_retained->Release();	// retained by result_SP
		pattern_uses_integer_keys = true;
	}
	else
	{
		pattern = (EidosDictionaryUnretained *)pattern_value->ObjectData()[0];
		result_SP = p_arguments[1];
		pattern_uses_integer_keys = pattern->KeysAreIntegers();
	}
	
	// Get the offspring sex -- actually we just need to check it here, and then sex_value is passed to _ValidateHaplosomesAndChooseSex() below
	//IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() == EidosValueType::kValueString)
	{
		const std::string &sex_string = sex_value->StringData()[0];
		
		if (sex_string.compare("M") == 0)
			; //sex = IndividualSex::kMale;
		else if (sex_string.compare("F") == 0)
			; //sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires sex to be 'M' or 'F', or NULL." << EidosTerminate();
	}
	
	// make a new inheritance dictionary and add it to pattern
	EidosDictionaryRetained *inheritance = new EidosDictionaryRetained();
	EidosValue_SP inheritance_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(inheritance, gEidosDictionaryRetained_Class));
	inheritance->Release();
	
	if (pattern_uses_integer_keys)
		pattern->SetKeyValue_IntegerKeys(chromosome->ID(), inheritance_SP);
	else
		pattern->SetKeyValue_StringKeys(chromosome->Symbol(), inheritance_SP);
	
	//
	//	the above code is shared with the other addPatternFor...() methods; the remainder of the code is not
	//
	
	// Get the strands for recombination and validate them
	Haplosome *strand1 = nullptr, *strand2 = nullptr, *strand3 = nullptr, *strand4 = nullptr;
	
	if (strand1_value->Type() != EidosValueType::kValueNULL)
		strand1 = (Haplosome *)strand1_value->ObjectData()[0];
	if (strand2_value->Type() != EidosValueType::kValueNULL)
		strand2 = (Haplosome *)strand2_value->ObjectData()[0];
	if (strand3_value->Type() != EidosValueType::kValueNULL)
		strand3 = (Haplosome *)strand3_value->ObjectData()[0];
	if (strand4_value->Type() != EidosValueType::kValueNULL)
		strand4 = (Haplosome *)strand4_value->ObjectData()[0];
	
	// SPECIES CONSISTENCY CHECK
	if (strand1 && (&strand1->OwningIndividual()->subpopulation_->species_ != this))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand1 belong to the target species." << EidosTerminate();
	if (strand2 && (&strand2->OwningIndividual()->subpopulation_->species_ != this))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand2 belong to the target species." << EidosTerminate();
	if (strand3 && (&strand3->OwningIndividual()->subpopulation_->species_ != this))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand3 belong to the target species." << EidosTerminate();
	if (strand4 && (&strand4->OwningIndividual()->subpopulation_->species_ != this))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand4 belong to the target species." << EidosTerminate();
	
	if (strand1 && strand1->chromosome_index_ != chromosome_index)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand1 belong to the specified chromosome." << EidosTerminate();
	if (strand2 && strand2->chromosome_index_ != chromosome_index)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand2 belong to the specified chromosome." << EidosTerminate();
	if (strand3 && strand3->chromosome_index_ != chromosome_index)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand3 belong to the specified chromosome." << EidosTerminate();
	if (strand4 && strand4->chromosome_index_ != chromosome_index)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addPatternForRecombinant): addPatternForRecombinant() requires that strand4 belong to the specified chromosome." << EidosTerminate();
	
	// validate the haplosome pattern given the chromosome type and sex
	bool haplosome1_null = (!strand1 && !strand2);
	bool haplosome2_null = (!strand3 && !strand4);
	bool make_second_haplosome = false;
	
	if ((chromosome_type == ChromosomeType::kA_DiploidAutosome) ||
		(chromosome_type == ChromosomeType::kX_XSexChromosome) ||
		(chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
		(chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull))
		make_second_haplosome = true;
	
	if (!haplosome2_null && !make_second_haplosome)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addPatternForRecombinant): for chromosome type '" << chromosome_type <<"', addPatternForRecombinant() requires that the second offspring haplosome is configured to be a null haplosome (since chromosome type '" << chromosome_type << "' is intrinsically haploid)." << EidosTerminate();
	
	int breaks1count = breaks1_value->Count(), breaks2count = breaks2_value->Count();
	
	if (breaks1count != 0)
	{
		if (haplosome1_null)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addPatternForRecombinant): with a NULL strand1 and strand2, breaks1 must be NULL or empty." << EidosTerminate();
		else if (!strand2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addPatternForRecombinant): non-empty breaks1 supplied with a NULL strand2; recombination between strand1 and strand2 is not possible, so breaks1 must be NULL or empty." << EidosTerminate();
	}
	if (breaks2count != 0)
	{
		if (haplosome2_null)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addPatternForRecombinant): with a NULL strand3 and strand4, breaks2 must be NULL or empty." << EidosTerminate();
		else if (!strand4)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addPatternForRecombinant): non-empty breaks2 supplied with a NULL strand4; recombination between strand3 and strand4 is not possible, so breaks2 must be NULL or empty." << EidosTerminate();
	}
	
	Subpopulation::_ValidateHaplosomesAndChooseSex(chromosome_type, haplosome1_null, haplosome2_null, sex_value, sex_enabled_, "addPatternForRecombinant()");
	
	// randomize strands if requested
	eidos_logical_t randomizeStrands = randomizeStrands_value->LogicalData()[0];
	
	if (randomizeStrands)
	{
		Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
		
		if (strand1 && strand2 && Eidos_RandomBool(rng_state))
			std::swap(strand1, strand2);
		if (strand3 && strand4 && Eidos_RandomBool(rng_state))
			std::swap(strand3, strand4);
	}
	
	// set the validated inheritance pattern into the dictionary
	if (strand1)	inheritance->SetKeyValue_StringKeys(gStr_strand1, p_arguments[2]);
	if (strand2)	inheritance->SetKeyValue_StringKeys(gStr_strand2, p_arguments[3]);
	if (breaks1_value->Type() != EidosValueType::kValueNULL)
		inheritance->SetKeyValue_StringKeys(gStr_breaks1, p_arguments[4]);
	
	if (strand3)	inheritance->SetKeyValue_StringKeys(gStr_strand3, p_arguments[5]);
	if (strand4)	inheritance->SetKeyValue_StringKeys(gStr_strand4, p_arguments[6]);
	if (breaks2_value->Type() != EidosValueType::kValueNULL)
		inheritance->SetKeyValue_StringKeys(gStr_breaks2, p_arguments[7]);
	
	pattern->ContentsChanged("Dictionary()");
	return result_SP;
}

//	*********************	– (object<Subpopulation>$)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio = 0.5], [l$ haploid = F])
//
EidosValue_SP Species::ExecuteMethod_addSubpop(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpop): addSubpop() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpop): addSubpop() may not be called from inside a callback." << EidosTerminate();
	
	EidosValue *subpopID_value = p_arguments[0].get();
	EidosValue *size_value = p_arguments[1].get();
	EidosValue *sexRatio_value = p_arguments[2].get();
	EidosValue *haploid_value = p_arguments[3].get();
	
	slim_objectid_t subpop_id = SLiM_ExtractObjectIDFromEidosValue_is(subpopID_value, 0, 'p');
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(size_value->IntAtIndex_NOCAST(0, nullptr));
	
	double sex_ratio = sexRatio_value->FloatAtIndex_NOCAST(0, nullptr);
	
	if ((sex_ratio != 0.5) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpop): addSubpop() sex ratio supplied in non-sexual simulation." << EidosTerminate();
	
	bool haploid = haploid_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if (haploid)
	{
		if (model_type_ == SLiMModelType::kModelTypeWF)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpop): addSubpop() cannot create haploid individuals with the haploid=T option in WF models." << EidosTerminate();
		// BCH 12/23/2024: For a brief time I also raised an error here if explicit chromosomes had been defined,
		// but then I realized that this flag remains useful in models of haplodiploidy, where you still want a
		// diploid chromosome (type "A") and want some individuals to have a null second haplosome.
	}
	
	// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
	Subpopulation *new_subpop = population_.AddSubpopulation(subpop_id, subpop_size, sex_ratio, haploid);
	
	// define a new Eidos variable to refer to the new subpopulation
	EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpop): addSubpop() symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
	
	return symbol_entry.second;
}

// WF only:
//	*********************	– (object<Subpopulation>$)addSubpopSplit(is$ subpopID, integer$ size, io<Subpopulation>$ sourceSubpop, [float$ sexRatio = 0.5])
//
EidosValue_SP Species::ExecuteMethod_addSubpopSplit(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpopSplit): addSubpopSplit() is not available in nonWF models." << EidosTerminate();
	
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpopSplit): addSubpopSplit() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpopSplit): addSubpopSplit() may not be called from inside a callback." << EidosTerminate();
	
	EidosValue *subpopID_value = p_arguments[0].get();
	EidosValue *size_value = p_arguments[1].get();
	EidosValue *sourceSubpop_value = p_arguments[2].get();
	EidosValue *sexRatio_value = p_arguments[3].get();
	
	slim_objectid_t subpop_id = SLiM_ExtractObjectIDFromEidosValue_is(subpopID_value, 0, 'p');
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(size_value->IntAtIndex_NOCAST(0, nullptr));
	Subpopulation *source_subpop = SLiM_ExtractSubpopulationFromEidosValue_io(sourceSubpop_value, 0, &community_, this, "addSubpopSplit()");		// SPECIES CONSISTENCY CHECK
	
	double sex_ratio = sexRatio_value->FloatAtIndex_NOCAST(0, nullptr);
	
	if ((sex_ratio != 0.5) && !sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpopSplit): addSubpopSplit() sex ratio supplied in non-sexual simulation." << EidosTerminate();
	
	// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
	Subpopulation *new_subpop = population_.AddSubpopulationSplit(subpop_id, *source_subpop, subpop_size, sex_ratio);
	
	// define a new Eidos variable to refer to the new subpopulation
	EidosSymbolTableEntry &symbol_entry = new_subpop->SymbolTableEntry();
	
	if (p_interpreter.SymbolTable().ContainsSymbol(symbol_entry.first))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_addSubpopSplit): addSubpopSplit() symbol " << EidosStringRegistry::StringForGlobalStringID(symbol_entry.first) << " was already defined prior to its definition here." << EidosTerminate();
	
	community_.SymbolTable().InitializeConstantSymbolEntry(symbol_entry);
	
	return symbol_entry.second;
}

//  *********************	– chromosomesOfType(string$ type)
EidosValue_SP Species::ExecuteMethod_chromosomesOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *type_value = p_arguments[0].get();
	std::string type_string = type_value->StringAtIndex_NOCAST(0, nullptr);
	ChromosomeType chromosome_type = ChromosomeTypeForString(type_string);
	
	// count the number of chromosomes of the requested type
	int chromosome_count = 0;
	
	for (Chromosome *chromosome : Chromosomes())
		if (chromosome->Type() == chromosome_type)
			chromosome_count++;
	
	// gather and return the matches
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Chromosome_Class))->reserve(chromosome_count);	// reserve enough space for all results
	
	for (Chromosome *chromosome : Chromosomes())
		if (chromosome->Type() == chromosome_type)
			result->push_object_element_no_check_RR(chromosome);
	
	return EidosValue_SP(result);
}

//  *********************	– chromosomesWithIDs(integer ids)
EidosValue_SP Species::ExecuteMethod_chromosomesWithIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *ids_value = p_arguments[0].get();
	int ids_count = ids_value->Count();
	
	if (ids_count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Chromosome_Class));
	
	const int64_t *ids_data = ids_value->IntData();
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Chromosome_Class))->reserve(ids_count);	// reserve enough space for all results
	
	for (int ids_index = 0; ids_index < ids_count; ids_index++)
	{
		int64_t id = ids_data[ids_index];
		Chromosome *chromosome = ChromosomeFromID(id);
		
		if (!chromosome)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_chromosomesWithIDs): chromosomesWithIDs() could not find a chromosome with the given id (" << id << ")." << EidosTerminate();
		
		result->push_object_element_no_check_RR(chromosome);
	}
	
	return EidosValue_SP(result);
}

//  *********************	– chromosomesWithSymbols(string symbols)
EidosValue_SP Species::ExecuteMethod_chromosomesWithSymbols(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *symbols_value = p_arguments[0].get();
	int symbols_count = symbols_value->Count();
	
	if (symbols_count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Chromosome_Class));
	
	const std::string *symbols_data = symbols_value->StringData();
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Chromosome_Class))->reserve(symbols_count);	// reserve enough space for all results
	
	for (int symbols_index = 0; symbols_index < symbols_count; symbols_index++)
	{
		const std::string &symbol = symbols_data[symbols_index];
		Chromosome *chromosome = ChromosomeFromSymbol(symbol);
		
		if (!chromosome)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_chromosomesWithSymbols): chromosomesWithSymbols() could not find a chromosome with the given symbol (" << symbol << ")." << EidosTerminate();
		
		result->push_object_element_no_check_RR(chromosome);
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (object<Individual>)individualsWithPedigreeIDs(integer pedigreeIDs, [Nio<Subpopulation> subpops = NULL])
EidosValue_SP Species::ExecuteMethod_individualsWithPedigreeIDs(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	if (!PedigreesEnabledByUser())
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_individualsWithPedigreeIDs): individualsWithPedigreeIDs() may only be called when pedigree recording has been enabled." << EidosTerminate();
	
	EidosValue *pedigreeIDs_value = p_arguments[0].get();
	EidosValue *subpops_value = p_arguments[1].get();
	
	// Cache the subpops across which we will tally
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::ExecuteMethod_individualsWithPedigreeIDs(): usage of statics");
	
	static std::vector<Subpopulation*> subpops_to_search;	// use a static to prevent allocation thrash
	subpops_to_search.resize(0);
	
	if (subpops_value->Type() == EidosValueType::kValueNULL)
	{
		// Search through all subpops
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
			subpops_to_search.emplace_back(subpop_pair.second);
	}
	else
	{
		// Search through specified subpops
		int requested_subpop_count = subpops_value->Count();
		
		for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
			subpops_to_search.emplace_back(SLiM_ExtractSubpopulationFromEidosValue_io(subpops_value, requested_subpop_index, &community_, this, "individualsWithPedigreeIDs()"));		// SPECIES CONSISTENCY CHECK
	}
	
	// An empty pedigreeIDs vector gets you an empty result, guaranteed
	int pedigreeIDs_count = pedigreeIDs_value->Count();
	
	if (pedigreeIDs_count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
	
	// Assemble the result
	const int64_t *pedigree_id_data = pedigreeIDs_value->IntData();
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(pedigreeIDs_count);	// reserve enough space for all results
	
	if (pedigreeIDs_count < 30)		// crossover point determined by timing tests on macOS with various subpop sizes; 30 seems good, although it will vary across platforms etc.
	{
		// for smaller problem sizes, we do sequential search for each pedigree ID
		for (int value_index = 0; value_index < pedigreeIDs_count; ++value_index)
		{
			slim_pedigreeid_t pedigree_id = pedigree_id_data[value_index];
			
			for (Subpopulation *subpop : subpops_to_search)
			{
				std::vector<Individual *> &inds = subpop->parent_individuals_;
				
				for (Individual *ind : inds)
				{
					if (ind->PedigreeID() == pedigree_id)
					{
						result->push_object_element_no_check_NORR(ind);
						goto foundMatch;
					}
				}
			}
			
			// Either we drop through to here, if we didn't find a match, or we goto to here, if we found one
		foundMatch:
			continue;
		}
	}
	else
	{
		// for larger problem sizes, we speed up lookups by building a hash table first, changing from O(N*M) to O(N)
		// we could get even more fancy and cache this hash table to speed up successive calls within one cycle,
		// but since the hash table is specific to the set of subpops we're searching, that would get a bit hairy...
#if EIDOS_ROBIN_HOOD_HASHING
		robin_hood::unordered_flat_map<slim_pedigreeid_t, Individual *> fromIDToIndividual;
		//typedef robin_hood::pair<slim_pedigreeid_t, Individual *> MAP_PAIR;
#elif STD_UNORDERED_MAP_HASHING
		std::unordered_map<slim_pedigreeid_t, Individual *> fromIDToIndividual;
		//typedef std::pair<slim_pedigreeid_t, Individual *> MAP_PAIR;
#endif
		
		try {
			for (Subpopulation *subpop : subpops_to_search)
			{
				std::vector<Individual *> &inds = subpop->parent_individuals_;
				
				for (Individual *ind : inds)
					fromIDToIndividual.emplace(ind->PedigreeID(), ind);
			}
		} catch (...) {
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_individualsWithPedigreeIDs): (internal error) SLiM encountered a raise from an internal hash table; please report this." << EidosTerminate(nullptr);
		}
		
#ifdef _OPENMP
		if (pedigreeIDs_count >= EIDOS_OMPMIN_INDS_W_PEDIGREE_IDS)
		{
			// separate parallel implementation, since the logic is somewhat different
			result->resize_no_initialize(pedigreeIDs_count);

			Individual **result_data = (Individual **)result->data();
			bool any_unmatched = false;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_INDS_W_PEDIGREE_IDS);
#pragma omp parallel for schedule(static) default(none) shared(pedigreeIDs_count, fromIDToIndividual) firstprivate(pedigree_id_data, result_data) reduction(||: any_unmatched) num_threads(thread_count) // if(EIDOS_OMPMIN_INDS_W_PEDIGREE_IDS) is above
			for (int value_index = 0; value_index < pedigreeIDs_count; ++value_index)
			{
				auto find_iter = fromIDToIndividual.find(pedigree_id_data[value_index]);
				
				if (find_iter != fromIDToIndividual.end())
				{
					result_data[value_index] = find_iter->second;
				}
				else
				{
					result_data[value_index] = nullptr;
					any_unmatched = true;
				}
			}
			
			// because of the parallelization, we had to insert nullptrs into the result vector and then compact it afterwards
			// this compaction needs to preserve order, so it shifts elements down rather than backfilling from the end
			if (any_unmatched)
			{
				int next_unfilled_index = 0;
				
				for (int value_index = 0; value_index < pedigreeIDs_count; ++value_index)
				{
					Individual *result_ind = result_data[value_index];
					
					if (result_ind != nullptr)
					{
						if (value_index != next_unfilled_index)
							result_data[next_unfilled_index] = result_ind;
						
						next_unfilled_index++;
					}
				}
				
				result->resize_no_initialize(next_unfilled_index);
			}
		}
		else
#endif
		{
			for (int value_index = 0; value_index < pedigreeIDs_count; ++value_index)
			{
				auto find_iter = fromIDToIndividual.find(pedigree_id_data[value_index]);
				
				if (find_iter != fromIDToIndividual.end())
					result->push_object_element_no_check_NORR(find_iter->second);
			}
		}
	}
	
	return EidosValue_SP(result);
}

//	*********************	- (void)killIndividuals(object<Individual> individuals)
//
EidosValue_SP Species::ExecuteMethod_killIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_killIndividuals): killIndividuals() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.executing_species_ == this)
		if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_killIndividuals): killIndividuals() must be called directly from a first(), early(), or late() event, when called on the currently executing species." << EidosTerminate();
	
	EidosValue_Object *individuals_value = (EidosValue_Object *)p_arguments[0].get();
	int individuals_count = individuals_value->Count();
	int killed_count = 0;
	
	if (individuals_count == 0)
		return gStaticEidosValueVOID;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(individuals_value);
	
	if (species != this)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_killIndividuals): killIndividuals() requires that all individuals belong to the same species as the target species." << EidosTerminate();
	
	// Loop over the individuals and kill them one by one; since there might be references to them in script, we can't actually
	// free the objects now, so we move them to a temporary "graveyard" which we dispose of between tick cycle stages
	Individual * const *individuals_data = (Individual * const *)individuals_value->ObjectData();
	
	for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
	{
		Individual *doomed = individuals_data[individual_index];
		slim_popsize_t source_subpop_index = doomed->index_;
		
		if (source_subpop_index < 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_killIndividuals): killIndividuals() may not kill an individual that is not visible in a subpopulation.  This error will occur if you try to kill the same individual more than once." << EidosTerminate();
		
		Subpopulation *source_subpop = doomed->subpopulation_;
		slim_popsize_t source_subpop_size = source_subpop->parent_subpop_size_;
		
		// remove the originals from source_subpop's vectors
		if (doomed->sex_ == IndividualSex::kFemale)
		{
			// females have to be backfilled by the last female, and then that hole is backfilled by a male, and the first male index changes
			slim_popsize_t source_first_male = source_subpop->parent_first_male_index_;
			
			if (source_subpop_index < source_first_male - 1)
			{
				Individual *backfill = source_subpop->parent_individuals_[source_first_male - 1];
				
				source_subpop->parent_individuals_[source_subpop_index] = backfill;
				backfill->index_ = source_subpop_index;
			}
			
			if (source_first_male - 1 < source_subpop_size - 1)
			{
				Individual *backfill = source_subpop->parent_individuals_[source_subpop_size - 1];
				
				source_subpop->parent_individuals_[source_first_male - 1] = backfill;
				backfill->index_ = source_first_male - 1;
			}
			
			source_subpop->parent_subpop_size_ = --source_subpop_size;
			source_subpop->parent_individuals_.resize(source_subpop_size);
			
			source_subpop->parent_first_male_index_ = --source_first_male;
		}
		else
		{
			// males and hermaphrodites can be removed with a simple backfill from the end of the vector
			if (source_subpop_index < source_subpop_size - 1)
			{
				Individual *backfill = source_subpop->parent_individuals_[source_subpop_size - 1];
				
				source_subpop->parent_individuals_[source_subpop_index] = backfill;
				backfill->index_ = source_subpop_index;
			}
			
			source_subpop->parent_subpop_size_ = --source_subpop_size;
			source_subpop->parent_individuals_.resize(source_subpop_size);
		}
		
		// add the doomed individual to our temporary graveyard
		graveyard_.push_back(doomed);
		
		// it gets killed_ of true and an index of -1; we need to be careful about these possible values where we need to distinguish killed individuals
		// note that we do not change the subpopulation_ pointer, even though we have removed it from the subpopulation!  this is a similar state to
		// new offspring, which also get an index of -1 and are not added to the subpopulation's main data structures yet; the reason not to set
		// the subpopulation_ to nullptr is that we still need to be able to use subpopulation_ to get to species_ and community_ for various purposes
		// we hide this from the user, though; accessing the subpopulation property on a killed individual raises an error
		doomed->killed_ = true;
		doomed->index_ = -1;
		
		killed_count++;
	}
	
	if (killed_count)
	{
		// First, clear our individual caches in all subpopulations; any subpops involved in
		// this method would be invalidated anyway so this probably isn't even that much overkill in
		// most models.
		for (auto subpop_pair : population_.subpops_)
			subpop_pair.second->cached_parent_individuals_value_.reset();
		
		// Invalidate interactions; we just do this for all subpops, for now, rather than trying to
		// selectively invalidate only the subpops involved in the deaths that occurred
		community_.InvalidateInteractionsForSpecies(this);
		
		// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
		population_.InvalidateMutationReferencesCache();
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	– (float)mutationFrequencies(Nio<Subpopulation> subpops, [No<Mutation> mutations = NULL])
//	*********************	– (integer)mutationCounts(Nio<Subpopulation> subpops, [No<Mutation> mutations = NULL])
//
EidosValue_SP Species::ExecuteMethod_mutationFreqsCounts(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_interpreter)
	EidosValue *subpops_value = p_arguments[0].get();
	EidosValue *mutations_value = p_arguments[1].get();
	
	// tally across the requested subpops; total haplosome counts are put into the chromosomes
	if (subpops_value->Type() == EidosValueType::kValueNULL)
	{
		// tally across the whole population
		population_.TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ false);
	}
	else
	{
		// requested subpops, so get them
		int requested_subpop_count = subpops_value->Count();
		
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("Species::ExecuteMethod_mutationFreqsCounts(): usage of statics");
		
		static std::vector<Subpopulation*> subpops_to_tally;	// using and clearing a static prevents allocation thrash; should be safe from re-entry
		
		subpops_to_tally.resize(0);
		
		if (requested_subpop_count)
		{
			for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
				subpops_to_tally.emplace_back(SLiM_ExtractSubpopulationFromEidosValue_io(subpops_value, requested_subpop_index, &community_, this,
																						 (p_method_id == gID_mutationFrequencies) ? "mutationFrequencies()" : "mutationCounts()"));		// SPECIES CONSISTENCY CHECK
			
			// unique subpops_to_tally so duplicates don't confuse the count
			std::sort(subpops_to_tally.begin(), subpops_to_tally.end());
			subpops_to_tally.resize(static_cast<size_t>(std::distance(subpops_to_tally.begin(), std::unique(subpops_to_tally.begin(), subpops_to_tally.end()))));
		}
		
		// If *all* subpops were requested, then we delegate to the method that is designed to tally across the whole population.
		// Since we uniqued the subpops_to_tally vector above, we can check for equality by just comparing sizes.
		if (subpops_to_tally.size() == population_.subpops_.size())
			population_.TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ false);
		else
			population_.TallyMutationReferencesAcrossSubpopulations(&subpops_to_tally);
	}
	
	// SPECIES CONSISTENCY CHECK
	if (mutations_value->Count() >= 1)
	{
		Species *mut_species = Community::SpeciesForMutations(mutations_value);
		
		if (mut_species != this)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_mutationFreqsCounts): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() requires that all mutations belong to the target species." << EidosTerminate();
	}
	
	// OK, now construct our result vector from the tallies for just the requested mutations
	// We now have utility methods on Population that do this for us; we pass a denominator
	// of nullptr, which says the denominator is the total haplosome count for each chromosome
	if (p_method_id == gID_mutationFrequencies)
		return population_.Eidos_FrequenciesForTalliedMutations(mutations_value);
	else // p_method_id == gID_mutationCounts
		return population_.Eidos_CountsForTalliedMutations(mutations_value);
}

//	*********************	- (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Species::ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &community_, this, "mutationsOfType()");		// SPECIES CONSISTENCY CHECK
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	// track calls per cycle to Species::ExecuteMethod_mutationsOfType() and Species::ExecuteMethod_countOfMutationsOfType()
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
		EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->resize_no_initialize_RR(mutation_count);
		EidosValue_SP result_SP = EidosValue_SP(vec);
		
		for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
			vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + mutation_registry[mut_index], mut_index);
		
		return result_SP;
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(mut_block_ptr + first_match, gSLiM_Mutation_Class));
		}
		else
		{
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->resize_no_initialize_RR(match_count);
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
EidosValue_SP Species::ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &community_, this, "countOfMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	// track calls per cycle to Species::ExecuteMethod_mutationsOfType() and Species::ExecuteMethod_countOfMutationsOfType()
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
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_count));
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
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(match_count));
	}
}
			
//	*********************	– (void)outputFixedMutations([Ns$ filePath = NULL], [logical$ append=F], [logical$ objectTags=F])
//
EidosValue_SP Species::ExecuteMethod_outputFixedMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *append_value = p_arguments[1].get();
	EidosValue *objectTags_value = p_arguments[2].get();
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	// TIMING RESTRICTION
	if (!community_.warned_early_output_)
	{
		if ((community_.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community_.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Species::ExecuteMethod_outputFixedMutations): outputFixedMutations() should probably not be called from a first() or early() event in a WF model; the output will reflect state at the beginning of the cycle, not the end." << std::endl;
				community_.warned_early_output_ = true;
			}
		}
	}
	
	std::ofstream outfile;
	bool has_file = false;
	std::string outfile_path;
	
	if (filePath_value->Type() != EidosValueType::kValueNULL)
	{
		outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
		bool append = append_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		has_file = true;
		
		if (!outfile.is_open())
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_outputFixedMutations): outputFixedMutations() could not open "<< outfile_path << "." << EidosTerminate();
	}
	else
	{
		// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
		Eidos_EraseProgress();
	}
	
	std::ostream &out = *(has_file ? dynamic_cast<std::ostream *>(&outfile) : dynamic_cast<std::ostream *>(&output_stream));
	
#if DO_MEMORY_CHECKS
	// This method can burn a huge amount of memory and get us killed, if we have a maximum memory usage.  It's nice to
	// try to check for that and terminate with a proper error message, to help the user diagnose the problem.
	int mem_check_counter = 0, mem_check_mod = 100;
	
	if (eidos_do_memory_checks)
		Eidos_CheckRSSAgainstMax("Species::ExecuteMethod_outputFixedMutations", "(outputFixedMutations(): The memory usage was already out of bounds on entry.)");
#endif
	
	// Output header line.  BCH 3/6/2022: Note that the cycle was added after the tick in SLiM 4.
	out << "#OUT: " << community_.Tick() << " " << Cycle() << " F";
	
	if (has_file)
		out << " " << outfile_path;
	
	out << std::endl;
	
	// Output Mutations section
	out << "Mutations:" << std::endl;
	
	bool output_object_tags = objectTags_value->LogicalAtIndex_NOCAST(0, nullptr);
	std::vector<Substitution*> &subs = population_.substitutions_;
	
	for (unsigned int i = 0; i < subs.size(); i++)
	{
		out << i << " ";
		
		if (output_object_tags)
			subs[i]->PrintForSLiMOutput_Tag(out);
		else
			subs[i]->PrintForSLiMOutput(out);
		
#if DO_MEMORY_CHECKS
		if (eidos_do_memory_checks)
		{
			mem_check_counter++;
			
			if (mem_check_counter % mem_check_mod == 0)
				Eidos_CheckRSSAgainstMax("Species::ExecuteMethod_outputFixedMutations", "(outputFixedMutations(): Out of memory while outputting substitution objects.)");
		}
#endif
	}
	
	if (has_file)
		outfile.close(); 
	
	return gStaticEidosValueVOID;
}
			
//	*********************	– (void)outputFull([Ns$ filePath = NULL], [logical$ binary = F], [logical$ append=F], [logical$ spatialPositions = T], [logical$ ages = T], [logical$ ancestralNucleotides = T], [logical$ pedigreeIDs = F], [logical$ objectTags = F], [logical$ substitutions = F])
//
EidosValue_SP Species::ExecuteMethod_outputFull(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *binary_value = p_arguments[1].get();
	EidosValue *append_value = p_arguments[2].get();
	EidosValue *spatialPositions_value = p_arguments[3].get();
	EidosValue *ages_value = p_arguments[4].get();
	EidosValue *ancestralNucleotides_value = p_arguments[5].get();
	EidosValue *pedigreeIDs_value = p_arguments[6].get();
	EidosValue *objectTags_value = p_arguments[7].get();
	EidosValue *substitutions_value = p_arguments[8].get();
	
	// TIMING RESTRICTION
	if (!community_.warned_early_output_)
	{
		if ((community_.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community_.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Species::ExecuteMethod_outputFull): outputFull() should probably not be called from a first() or early() event in a WF model; the output will reflect state at the beginning of the cycle, not the end." << std::endl;
				community_.warned_early_output_ = true;
			}
		}
	}
	
	bool use_binary = binary_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_spatial_positions = spatialPositions_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_ages = ages_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_ancestral_nucs = ancestralNucleotides_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_pedigree_ids = pedigreeIDs_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_object_tags = objectTags_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool output_substitutions = substitutions_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if (output_pedigree_ids && !PedigreesEnabledByUser())
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_outputFull): outputFull() cannot output pedigree IDs, because pedigree recording has not been enabled." << EidosTerminate();
	
	// BCH 3/6/2022: Note that in SLiM 4 we now output the species cycle after the tick.  This breaks backward compatibility
	// for code that parses the output from outputFull(), but in a minor way.  It is necessary so that we can round-trip a model
	// with outputFull()/readFromPopulationFile(); that needs to restore the species cycle.  The cycle is also added to
	// the other text output formats, except those on Haplosome (where the haplosomes might come from multiple species).
	
	if (filePath_value->Type() == EidosValueType::kValueNULL)
	{
		if (use_binary)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_outputFull): outputFull() cannot output in binary format to the standard output stream; specify a file for output." << EidosTerminate();
		
		// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
		Eidos_EraseProgress();
		
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		Individual::PrintIndividuals_SLiM(output_stream, nullptr, 0, *this, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids, output_object_tags, output_substitutions, /* p_focal_chromosome */ nullptr);
	}
	else
	{
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
		bool append = append_value->LogicalAtIndex_NOCAST(0, nullptr);
		std::ofstream outfile;
		
		if (use_binary && append)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_outputFull): outputFull() cannot append in binary format." << EidosTerminate();
		
		if (use_binary)
			outfile.open(outfile_path.c_str(), std::ios::out | std::ios::binary);
		else
			outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (outfile.is_open())
		{
			if (use_binary)
			{
				population_.PrintAllBinary(outfile, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids, output_object_tags, output_substitutions);
			}
			else
			{
				Individual::PrintIndividuals_SLiM(outfile, nullptr, 0, *this, output_spatial_positions, output_ages, output_ancestral_nucs, output_pedigree_ids, output_object_tags, output_substitutions, /* p_focal_chromosome */ nullptr);
			}
			
			outfile.close(); 
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_outputFull): outputFull() could not open "<< outfile_path << "." << EidosTerminate();
		}
	}
	
	return gStaticEidosValueVOID;
}
			
//	*********************	– (void)outputMutations(object<Mutation> mutations, [Ns$ filePath = NULL], [logical$ append=F], [logical$ objectTags=F])
//
EidosValue_SP Species::ExecuteMethod_outputMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	EidosValue *filePath_value = p_arguments[1].get();
	EidosValue *append_value = p_arguments[2].get();
	EidosValue *objectTags_value = p_arguments[3].get();
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	// TIMING RESTRICTION
	if (!community_.warned_early_output_)
	{
		if ((community_.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community_.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Species::ExecuteMethod_outputMutations): outputMutations() should probably not be called from a first() or early() event in a WF model; the output will reflect state at the beginning of the cycle, not the end." << std::endl;
				community_.warned_early_output_ = true;
			}
		}
	}
	
	std::ofstream outfile;
	bool has_file = false;
	
	if (filePath_value->Type() != EidosValueType::kValueNULL)
	{
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
		bool append = append_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		has_file = true;
		
		if (!outfile.is_open())
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_outputMutations): outputMutations() could not open "<< outfile_path << "." << EidosTerminate();
	}
	else
	{
		// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
		Eidos_EraseProgress();
	}
	
	std::ostream &out = *(has_file ? (std::ostream *)&outfile : (std::ostream *)&output_stream);
	
	int mutations_count = mutations_value->Count();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if (mutations_count > 0)
	{
		// SPECIES CONSISTENCY CHECK
		Species *mutations_species = Community::SpeciesForMutations(mutations_value);
		
		if (mutations_species != this)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_outputMutations): outputMutations() requires that all mutations belong to the target species." << EidosTerminate();
		
		// as we scan through haplosomes building the polymorphism map, we want to process only mutations that are
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
			Mutation *mut = (Mutation *)(mutations_object->ObjectElementAtIndex_NOCAST(mut_index, nullptr));
			mut->scratch_ = 1;
		}
		
		// find all polymorphisms of the mutations that are to be tracked
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			PolymorphismMap polymorphisms;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				int haplosome_count_per_individual = HaplosomeCountPerIndividual();
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = ind->haplosomes_[haplosome_index];
					
					int mutrun_count = haplosome->mutrun_count_;
					
					for (int run_index = 0; run_index < mutrun_count; ++run_index)
					{
						const MutationRun *mutrun = haplosome->mutruns_[run_index];
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
			}
			
			// output the frequencies of these mutations in each subpopulation; note the format here comes from the old tracked mutations code
			// NOTE the format of this output changed because print_no_id() added the mutation_id_ to its output; BCH 11 June 2016
			// BCH 3/6/2022: Note that the cycle was added after the tick in SLiM 4.
			bool output_object_tags = objectTags_value->LogicalAtIndex_NOCAST(0, nullptr);
			
			for (const PolymorphismPair &polymorphism_pair : polymorphisms) 
			{
				out << "#OUT: " << community_.Tick() << " " << Cycle() << " T p" << subpop_pair.first << " ";
				
				if (output_object_tags)
					polymorphism_pair.second.Print_NoID_Tag(out);
				else
					polymorphism_pair.second.Print_NoID(out);
			}
		}
	}
	
	if (has_file)
		outfile.close(); 
	
	return gStaticEidosValueVOID;
}

//	*********************	- (integer$)readFromPopulationFile(string$ filePath, [No<Dictionary>$ subpopMap = NULL])
//
EidosValue_SP Species::ExecuteMethod_readFromPopulationFile(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	// readFromPopulationFile() is strictly limited to first()/early()/late() events; it cannot be called
	// from other contexts even for a different species than executing_species_.  This is because
	// it can have the side effect of running mutationEffect() callbacks, and those cannot nest inside
	// the execution of a different species.
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) &&
		(cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) &&
		(cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_readFromPopulationFile): readFromPopulationFile() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) &&
		(community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) &&
		(community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_readFromPopulationFile): readFromPopulationFile() may not be called from inside a callback." << EidosTerminate();
	
	if (!community_.warned_early_read_)
	{
		if ((community_.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts) ||
			(community_.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Species::ExecuteMethod_readFromPopulationFile): readFromPopulationFile() should probably not be called from a first() or early() event in a WF model; fitness values will not be recalculated prior to generating offspring unless recalculateFitness() is called." << std::endl;
				community_.warned_early_read_ = true;
			}
		}
		// Note that there is no equivalent problem in nonWF models, because fitness values are used for survival,
		// not reproduction, and there is no event stage in the tick cycle that splits fitness from survival.
	}
	
	EidosValue *filePath_value = p_arguments[0].get();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex_NOCAST(0, nullptr)));
	
	EidosValue *subpopMap_value = p_arguments[1].get();
	SUBPOP_REMAP_HASH subpopRemap;
	
	if (subpopMap_value->Type() != EidosValueType::kValueNULL)
	{
		EidosDictionaryUnretained *subpopMap_dict = (EidosDictionaryUnretained *)subpopMap_value->ObjectElementAtIndex_NOCAST(0, nullptr);
		
		if (!subpopMap_dict->KeysAreStrings())
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_readFromPopulationFile): subpopMap must use strings for its keys; integer keys are not presently supported." << EidosTerminate();
		
		const EidosDictionaryHashTable_StringKeys *subpopMap_hash = subpopMap_dict->DictionarySymbols_StringKeys();
		
		for (auto &subpopMap_pair : *subpopMap_hash)
		{
			std::string slim_id_string = subpopMap_pair.first;
			slim_objectid_t slim_id = SLiMEidosScript::ExtractIDFromStringWithPrefix(slim_id_string, 'p', nullptr);
			EidosValue *table_id_value = subpopMap_pair.second.get();
			
			if ((table_id_value->Type() != EidosValueType::kValueInt) || (table_id_value->Count() != 1))
				EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_readFromPopulationFile): subpopMap values must be singleton integers." << EidosTerminate();
			
			int64_t table_id = table_id_value->IntAtIndex_NOCAST(0, nullptr);
			
			if ((table_id < 0) || (table_id > SLIM_MAX_ID_VALUE))
				EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_readFromPopulationFile): subpopMap value (" << table_id << ") is out of range." << EidosTerminate();
			
			if (subpopRemap.find(table_id) != subpopRemap.end())
				EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_readFromPopulationFile): subpopMap value (" << table_id << ") is not unique; more than one subpopulation id is mapped from it." << EidosTerminate();
			
			subpopRemap.emplace(std::pair<int64_t, slim_objectid_t>(table_id, slim_id));
		}
	}
	
	slim_tick_t file_tick = InitializePopulationFromFile(file_path, &p_interpreter, subpopRemap);
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(file_tick));
}
			
//	*********************	– (void)recalculateFitness([Ni$ tick = NULL])
//
EidosValue_SP Species::ExecuteMethod_recalculateFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	// recalculateFitness() is strictly limited to first()/early()/late() events; it cannot be called
	// from other contexts even for a different species than executing_species_.  This is because
	// it can have the side effect of running mutationEffect() callbacks, and those cannot nest inside
	// the execution of a different species.
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_recalculateFitness): recalculateFitness() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_recalculateFitness): recalculateFitness() may not be called from inside a callback." << EidosTerminate();
	
	EidosValue *tick_value = p_arguments[0].get();
	
	// Trigger a fitness recalculation.  This is suggested after making a change that would modify fitness values, such as altering
	// a selection coefficient or dominance coefficient, changing the mutation type for a mutation, etc.  It will have the side
	// effect of calling mutationEffect() callbacks, so this is quite a heavyweight operation.
	slim_tick_t tick = (tick_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(tick_value->IntAtIndex_NOCAST(0, nullptr)) : community_.Tick();
	
	population_.RecalculateFitness(tick);
	
	// Remember that we have recalculated fitness values; this unlocks the ability to call cachedFitness(), temporarily
	has_recalculated_fitness_ = true;
	
	return gStaticEidosValueVOID;
}

//	*********************	– (object<SLiMEidosBlock>$)registerFitnessEffectCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP Species::ExecuteMethod_registerFitnessEffectCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *subpop_value = p_arguments[2].get();
	EidosValue *start_value = p_arguments[3].get();
	EidosValue *end_value = p_arguments[4].get();
	
	slim_objectid_t script_id = -1;		// used if id_value is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex_NOCAST(0, nullptr);
	slim_objectid_t subpop_id = -1;		// used if subpop_value is NULL, to indicate applicability to all subpops
	slim_tick_t start_tick = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(start_value->IntAtIndex_NOCAST(0, nullptr)) : 1);
	slim_tick_t end_tick = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(end_value->IntAtIndex_NOCAST(0, nullptr)) : SLIM_MAX_TICK + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex_NOCAST(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex_NOCAST(0, nullptr))->subpopulation_id_;
	
	if (start_tick > end_tick)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerFitnessEffectCallback): registerFitnessEffectCallback() requires start <= end." << EidosTerminate();
	
	community_.CheckScheduling(start_tick, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage6CalculateFitness : SLiMCycleStage::kNonWFStage3CalculateFitness);
	
	SLiMEidosBlockType block_type = SLiMEidosBlockType::SLiMEidosFitnessEffectCallback;
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_tick, end_tick, this, nullptr);
	
	new_script_block->subpopulation_id_ = subpop_id;
	
	// SPECIES CONSISTENCY CHECK (done by AddScriptBlock())
	community_.AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerRecombinationCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Niso<Chromosome>$ chromosome = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//	*********************	– (object<SLiMEidosBlock>$)registerSurvivalCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP Species::ExecuteMethod_registerMateModifyRecSurvCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (p_method_id == gID_registerMateChoiceCallback)
		if (model_type_ == SLiMModelType::kModelTypeNonWF)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerMateModifyRecSurvCallback): registerMateChoiceCallback() is not available in nonWF models." << EidosTerminate();
	if (p_method_id == gID_registerSurvivalCallback)
		if (model_type_ == SLiMModelType::kModelTypeWF)
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerMateModifyRecSurvCallback): registerSurvivalCallback() is not available in WF models." << EidosTerminate();
	
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *subpop_value = p_arguments[2].get();
	EidosValue *start_value = p_arguments[(p_method_id == gID_registerRecombinationCallback) ? 4 : 3].get();
	EidosValue *end_value = p_arguments[(p_method_id == gID_registerRecombinationCallback) ? 5 : 4].get();
	
	slim_objectid_t script_id = -1;		// used if the id is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex_NOCAST(0, nullptr);
	slim_objectid_t subpop_id = -1;
	slim_tick_t start_tick = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(start_value->IntAtIndex_NOCAST(0, nullptr)) : 1);
	slim_tick_t end_tick = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(end_value->IntAtIndex_NOCAST(0, nullptr)) : SLIM_MAX_TICK + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex_NOCAST(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex_NOCAST(0, nullptr))->subpopulation_id_;
	
	if (start_tick > end_tick)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerMateModifyRecSurvCallback): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() requires start <= end." << EidosTerminate();
	
	SLiMEidosBlockType block_type;
	
	if (p_method_id == gID_registerMateChoiceCallback)					block_type = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
	else if (p_method_id == gID_registerModifyChildCallback)			block_type = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
	else if (p_method_id == gID_registerRecombinationCallback)			block_type = SLiMEidosBlockType::SLiMEidosRecombinationCallback;
	else if (p_method_id == gID_registerSurvivalCallback)				block_type = SLiMEidosBlockType::SLiMEidosSurvivalCallback;
	else
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerMateModifyRecSurvCallback): (internal error) unrecognized callback type." << EidosTerminate();
	
	community_.CheckScheduling(start_tick, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage2GenerateOffspring : SLiMCycleStage::kNonWFStage1GenerateOffspring);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_tick, end_tick, this, nullptr);
	
	new_script_block->subpopulation_id_ = subpop_id;
	
	// Get the focal chromosome, for recombination() callbacks
	if (p_method_id == gID_registerRecombinationCallback)
	{
		EidosValue *chromosome_value = p_arguments[3].get();
		Chromosome *chromosome = GetChromosomeFromEidosValue(chromosome_value);	// returns nullptr for NULL
		
		if (chromosome)
			new_script_block->chromosome_id_ = chromosome->ID();
	}
	
	// SPECIES CONSISTENCY CHECK (done by AddScriptBlock())
	community_.AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerMutationCallback(Nis$ id, string$ source, [Nio<MutationType>$ mutType = NULL], [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP Species::ExecuteMethod_registerMutationCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *mutType_value = p_arguments[2].get();
	EidosValue *subpop_value = p_arguments[3].get();
	EidosValue *start_value = p_arguments[4].get();
	EidosValue *end_value = p_arguments[5].get();
	
	slim_objectid_t script_id = -1;		// used if id_value is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex_NOCAST(0, nullptr);
	slim_objectid_t mut_type_id = -1;	// used if mutType_value is NULL, to indicate applicability to all mutation types
	slim_objectid_t subpop_id = -1;		// used if subpop_value is NULL, to indicate applicability to all subpops
	slim_tick_t start_tick = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(start_value->IntAtIndex_NOCAST(0, nullptr)) : 1);
	slim_tick_t end_tick = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(end_value->IntAtIndex_NOCAST(0, nullptr)) : SLIM_MAX_TICK + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (mutType_value->Type() != EidosValueType::kValueNULL)
		mut_type_id = (mutType_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(mutType_value->IntAtIndex_NOCAST(0, nullptr)) : ((MutationType *)mutType_value->ObjectElementAtIndex_NOCAST(0, nullptr))->mutation_type_id_;
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex_NOCAST(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex_NOCAST(0, nullptr))->subpopulation_id_;
	
	if (start_tick > end_tick)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerMutationCallback): registerMutationCallback() requires start <= end." << EidosTerminate();
	
	community_.CheckScheduling(start_tick, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage2GenerateOffspring : SLiMCycleStage::kNonWFStage1GenerateOffspring);
	
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, SLiMEidosBlockType::SLiMEidosMutationCallback, start_tick, end_tick, this, nullptr);
	
	new_script_block->mutation_type_id_ = mut_type_id;
	new_script_block->subpopulation_id_ = subpop_id;
	
	// SPECIES CONSISTENCY CHECK (done by AddScriptBlock())
	community_.AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerMutationEffectCallback(Nis$ id, string$ source, io<MutationType>$ mutType, [Nio<Subpopulation>$ subpop = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP Species::ExecuteMethod_registerMutationEffectCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *mutType_value = p_arguments[2].get();
	EidosValue *subpop_value = p_arguments[3].get();
	EidosValue *start_value = p_arguments[4].get();
	EidosValue *end_value = p_arguments[5].get();
	
	slim_objectid_t script_id = -1;		// used if id_value is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex_NOCAST(0, nullptr);
	slim_objectid_t mut_type_id = -1;
	slim_objectid_t subpop_id = -1;		// used if subpop_value is NULL, to indicate applicability to all subpops
	slim_tick_t start_tick = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(start_value->IntAtIndex_NOCAST(0, nullptr)) : 1);
	slim_tick_t end_tick = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(end_value->IntAtIndex_NOCAST(0, nullptr)) : SLIM_MAX_TICK + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	mut_type_id = (mutType_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(mutType_value->IntAtIndex_NOCAST(0, nullptr)) : ((MutationType *)mutType_value->ObjectElementAtIndex_NOCAST(0, nullptr))->mutation_type_id_;
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex_NOCAST(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex_NOCAST(0, nullptr))->subpopulation_id_;
	
	if (start_tick > end_tick)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerMutationEffectCallback): registerMutationEffectCallback() requires start <= end." << EidosTerminate();
	
	community_.CheckScheduling(start_tick, (model_type_ == SLiMModelType::kModelTypeWF) ? SLiMCycleStage::kWFStage6CalculateFitness : SLiMCycleStage::kNonWFStage3CalculateFitness);
	
	SLiMEidosBlockType block_type = SLiMEidosBlockType::SLiMEidosMutationEffectCallback;
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_tick, end_tick, this, nullptr);
	
	new_script_block->mutation_type_id_ = mut_type_id;
	new_script_block->subpopulation_id_ = subpop_id;
	
	// SPECIES CONSISTENCY CHECK (done by AddScriptBlock())
	community_.AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	– (object<SLiMEidosBlock>$)registerReproductionCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop = NULL], [Ns$ sex = NULL], [Ni$ start = NULL], [Ni$ end = NULL])
//
EidosValue_SP Species::ExecuteMethod_registerReproductionCallback(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerReproductionCallback): registerReproductionCallback() is not available in WF models." << EidosTerminate();
	
	EidosValue *id_value = p_arguments[0].get();
	EidosValue *source_value = p_arguments[1].get();
	EidosValue *subpop_value = p_arguments[2].get();
	EidosValue *sex_value = p_arguments[3].get();
	EidosValue *start_value = p_arguments[4].get();
	EidosValue *end_value = p_arguments[5].get();
	
	slim_objectid_t script_id = -1;		// used if the id is NULL, to indicate an anonymous block
	std::string script_string = source_value->StringAtIndex_NOCAST(0, nullptr);
	slim_objectid_t subpop_id = -1;
	IndividualSex sex_specificity = IndividualSex::kUnspecified;
	slim_tick_t start_tick = ((start_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(start_value->IntAtIndex_NOCAST(0, nullptr)) : 1);
	slim_tick_t end_tick = ((end_value->Type() != EidosValueType::kValueNULL) ? SLiMCastToTickTypeOrRaise(end_value->IntAtIndex_NOCAST(0, nullptr)) : SLIM_MAX_TICK + 1);
	
	if (id_value->Type() != EidosValueType::kValueNULL)
		script_id = SLiM_ExtractObjectIDFromEidosValue_is(id_value, 0, 's');
	
	if (subpop_value->Type() != EidosValueType::kValueNULL)
		subpop_id = (subpop_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(subpop_value->IntAtIndex_NOCAST(0, nullptr)) : ((Subpopulation *)subpop_value->ObjectElementAtIndex_NOCAST(0, nullptr))->subpopulation_id_;
	
	if (sex_value->Type() != EidosValueType::kValueNULL)
	{
		std::string sex_string = sex_value->StringAtIndex_NOCAST(0, nullptr);
		
		if (sex_string == "M")			sex_specificity = IndividualSex::kMale;
		else if (sex_string == "F")		sex_specificity = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerReproductionCallback): registerReproductionCallback() requires sex to be 'M', 'F', or NULL." << EidosTerminate();
		
		if (!SexEnabled())
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerReproductionCallback): registerReproductionCallback() requires sex to be NULL in non-sexual models." << EidosTerminate();
	}
	
	if (start_tick > end_tick)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_registerReproductionCallback): registerReproductionCallback() requires start <= end." << EidosTerminate();
	
	community_.CheckScheduling(start_tick, SLiMCycleStage::kNonWFStage1GenerateOffspring);
	
	SLiMEidosBlockType block_type = SLiMEidosBlockType::SLiMEidosReproductionCallback;
	SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_id, script_string, block_type, start_tick, end_tick, this, nullptr);
	
	new_script_block->subpopulation_id_ = subpop_id;
	new_script_block->sex_specificity_ = sex_specificity;
	
	// SPECIES CONSISTENCY CHECK (done by AddScriptBlock())
	community_.AddScriptBlock(new_script_block, &p_interpreter, nullptr);		// takes ownership from us
	
	return new_script_block->SelfSymbolTableEntry().second;
}

//	*********************	- (void)simulationFinished(void)
//
EidosValue_SP Species::ExecuteMethod_simulationFinished(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	if (community_.AllSpecies().size() != 1)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_simulationFinished): simulationFinished() may only be called on Species in single-species models; this is supported for backward compatibility.  In multispecies models, call community.simulationFinished() instead." << EidosTerminate();
	
	// Call through to our community to forward the message; note this means we must have an identical signature!
	community_.ExecuteMethod_simulationFinished(p_method_id, p_arguments, p_interpreter);
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)skipTick(void)
//
EidosValue_SP Species::ExecuteMethod_skipTick(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_skipTick): skipTick() may only be called from a first() event; skipping ticks should be arranged before any portion of the cycle has occurred." << EidosTerminate();
	
	if (species_active_)
	{
#ifdef SLIMGUI
		gSLiMScheduling << "\t\tspecies " << name_ << " DEACTIVATED by skipTick()" << std::endl;
#endif
		
		species_active_ = false;
		
		// deactivate all script blocks that have a "species" or "ticks" specifier in their declaration that refers to this species
		std::vector<SLiMEidosBlock*> &script_blocks = community_.AllScriptBlocks();
		
		for (SLiMEidosBlock *block : script_blocks)
			if ((block->species_spec_ && (block->species_spec_ == this)) || (block->ticks_spec_ && (block->ticks_spec_ == this)))
				block->block_active_ = 0;
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	- (object<Mutation>)subsetMutations([No<Mutation>$ exclude = NULL], [Nio<MutationType>$ mutationType = NULL], [Ni$ position = NULL], [Nis$ nucleotide = NULL], [Ni$ tag = NULL], [Ni$ id = NULL], [Niso<Chromosome>$ chromosome])
//
EidosValue_SP Species::ExecuteMethod_subsetMutations(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *exclude_value = p_arguments[0].get();
	EidosValue *mutType_value = p_arguments[1].get();
	EidosValue *position_value = p_arguments[2].get();
	EidosValue *nucleotide_value = p_arguments[3].get();
	EidosValue *tag_value = p_arguments[4].get();
	EidosValue *id_value = p_arguments[5].get();
	EidosValue *chromosome_value = p_arguments[6].get();
	
	// parse our arguments
	Mutation *exclude = (exclude_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Mutation *)exclude_value->ObjectElementAtIndex_NOCAST(0, nullptr);
	MutationType *mutation_type_ptr = (mutType_value->Type() == EidosValueType::kValueNULL) ? nullptr : SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &community_, this, "subsetMutations()");	// SPECIES CONSISTENCY CHECK
	slim_position_t position = (position_value->Type() == EidosValueType::kValueNULL) ? -1 : SLiMCastToPositionTypeOrRaise(position_value->IntAtIndex_NOCAST(0, nullptr));
	int8_t nucleotide = -1;
	bool has_tag = !(tag_value->Type() == EidosValueType::kValueNULL);
	slim_usertag_t tag = (has_tag ? tag_value->IntAtIndex_NOCAST(0, nullptr) : 0);
	bool has_id = !(id_value->Type() == EidosValueType::kValueNULL);
	slim_mutationid_t id = (has_id ? id_value->IntAtIndex_NOCAST(0, nullptr) : 0);
	bool has_chromosome = !(chromosome_value->Type() == EidosValueType::kValueNULL);
	Chromosome *chromosome = nullptr;
	slim_chromosome_index_t chromosome_index = 0;
	
	if (has_chromosome)		// NULL case handled above
	{
		chromosome = GetChromosomeFromEidosValue(chromosome_value);
		chromosome_index = chromosome->Index();
	}
	
	// SPECIES CONSISTENCY CHECK
	if (exclude && (&exclude->mutation_type_ptr_->species_ != this))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_subsetMutations): subsetMutations() requires that exclude belong to the target species." << EidosTerminate();
	if (chromosome && (&chromosome->species_ != this))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_subsetMutations): subsetMutations() requires that chromosome belong to the target species." << EidosTerminate();
	
	if (nucleotide_value->Type() == EidosValueType::kValueInt)
	{
		int64_t nuc_int = nucleotide_value->IntAtIndex_NOCAST(0, nullptr);
		
		if ((nuc_int < 0) || (nuc_int > 3))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_subsetMutations): subsetMutations() requires integer nucleotide values to be in [0,3]." << EidosTerminate();
		
		nucleotide = (int8_t)nuc_int;
	}
	else if (nucleotide_value->Type() == EidosValueType::kValueString)
	{
		const std::string &nuc_string = ((EidosValue_String *)nucleotide_value)->StringRefAtIndex_NOCAST(0, nullptr);
		
		if (nuc_string == "A")		nucleotide = 0;
		else if (nuc_string == "C")	nucleotide = 1;
		else if (nuc_string == "G")	nucleotide = 2;
		else if (nuc_string == "T")	nucleotide = 3;
		else EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_subsetMutations): subsetMutations() requires string nucleotide values to be 'A', 'C', 'G', or 'T'." << EidosTerminate();
	}
	
	// We will scan forward looking for a match, and will keep track of the first match we find.  If we only find one, we return
	// a singleton; if we find a second, we will start accumulating a vector result.
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int registry_size;
	const MutationIndex *registry = population_.MutationRegistry(&registry_size);
	int match_count = 0, registry_index;
	Mutation *first_match = nullptr;
	EidosValue_Object *vec = nullptr;
	
	if (has_id && !exclude && !mutation_type_ptr && (position == -1) && (nucleotide == -1) && !has_tag && !has_chromosome)
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
				vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
				vec->push_object_element_RR(first_match);
				vec->push_object_element_RR(mut);
			}
			else
			{
				vec->push_object_element_RR(mut);
			}
		}
	}
	else if (has_chromosome && !exclude && !mutation_type_ptr && (position == -1) && (nucleotide == -1) && !has_tag && !has_id)
	{
		// chromosome-only search; nice for this to be fast since people will use it to look up all the mutations for a chromosome
		for (registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			Mutation *mut = mut_block_ptr + registry[registry_index];
			
			if (mut->chromosome_index_ != chromosome_index)
				continue;
			
			match_count++;
			
			if (match_count == 1)
			{
				first_match = mut;
			}
			else if (match_count == 2)
			{
				vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
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
			if (has_chromosome && (mut->chromosome_index_ != chromosome_index))			continue;
			
			match_count++;
			
			if (match_count == 1)
			{
				first_match = mut;
			}
			else if (match_count == 2)
			{
				vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
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
			if (has_chromosome && (mut->chromosome_index_ != chromosome_index))			continue;
			
			match_count++;
			
			if (match_count == 1)
			{
				first_match = mut;
			}
			else if (match_count == 2)
			{
				vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
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
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
	else if (match_count == 1)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(first_match, gSLiM_Mutation_Class));
	else
		return EidosValue_SP(vec);
}

//	*********************	- (object<Substitution>)substitutionsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Species::ExecuteMethod_substitutionsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &community_, this, "mutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Substitution_Class));
	EidosValue_SP result_SP = EidosValue_SP(vec);
	
	std::vector<Substitution*> &substitutions = population_.substitutions_;
	int substitution_count = (int)substitutions.size();

	for (int sub_index = 0; sub_index < substitution_count; ++sub_index)
	{
		Substitution *sub = substitutions[sub_index];
		
		if (sub->mutation_type_ptr_ == mutation_type_ptr)
			vec->push_object_element_RR(sub);
	}
	
	return result_SP;
}

// TREE SEQUENCE RECORDING
//	*********************	- (logical$)treeSeqCoalesced(void)
//
EidosValue_SP Species::ExecuteMethod_treeSeqCoalesced(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqCoalesced): treeSeqCoalesced() may only be called when tree recording is enabled." << EidosTerminate();
	if (!running_coalescence_checks_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqCoalesced): treeSeqCoalesced() may only be called when coalescence checking is enabled; pass checkCoalescence=T to initializeTreeSeq() to enable this feature." << EidosTerminate();
	
	// This method now checks for *all* of the tree sequences being coalesced.  It could be extended to
	// take a [Niso<Chromosome>$ chromosome = NULL] parameter, to allow one chromosome to be checked.
	for (const TreeSeqInfo &tsinfo : treeseq_)
		if (tsinfo.last_coalescence_state_ == false)
			return gStaticEidosValue_LogicalF;
	
	return gStaticEidosValue_LogicalT;
}

// TREE SEQUENCE RECORDING
//	*********************	- (void)treeSeqSimplify(void)
//
EidosValue_SP Species::ExecuteMethod_treeSeqSimplify(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqSimplify): treeSeqSimplify() may only be called when tree recording is enabled." << EidosTerminate();
	
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqSimplify): treeSeqSimplify() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqSimplify): treeSeqSimplify() may not be called from inside a callback." << EidosTerminate();
	
	SimplifyAllTreeSequences();
	
	return gStaticEidosValueVOID;
}

// TREE SEQUENCE RECORDING
//	*********************	- (void)treeSeqRememberIndividuals(object<Individual> individuals, [logical$ permanent = T])
//
EidosValue_SP Species::ExecuteMethod_treeSeqRememberIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue_Object *individuals_value = (EidosValue_Object *)p_arguments[0].get();
    EidosValue *permanent_value = p_arguments[1].get();
	int ind_count = individuals_value->Count();
	
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqRememberIndividuals): treeSeqRememberIndividuals() may only be called when tree recording is enabled." << EidosTerminate();
	
	// TIMING RESTRICTION
	// BCH 14 November 2018: removed a block on calling treeSeqRememberIndividuals() from mutationEffect() callbacks,
	// because it turns out that can be useful (see correspondence with Yan Wong)
	// BCH 30 April 2019: also allowing mutation() callbacks, since I can see how that could be useful...
	if (community_.executing_species_ == this)
		if ((community_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosMateChoiceCallback) || (community_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback) || (community_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback))
			EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqRememberIndividuals): treeSeqRememberIndividuals() may not be called from inside a mateChoice(), modifyChild(), or recombination() callback for the currently executing species." << EidosTerminate();
	
	bool permanent = permanent_value->LogicalAtIndex_NOCAST(0, nullptr); 
	uint32_t flag = permanent ? SLIM_TSK_INDIVIDUAL_REMEMBERED : SLIM_TSK_INDIVIDUAL_RETAINED;
	
	if (ind_count == 0)
		return gStaticEidosValueVOID;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(individuals_value);
	
	if (species != this)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqRememberIndividuals): treeSeqRememberIndividuals() requires that all individuals belong to the target species." << EidosTerminate();
	
	// This method remembers the given individuals once, in the shared individuals table kept by treeseq_[0]
	EidosObject * const *oe_buffer = individuals_value->ObjectData();
	Individual * const *ind_buffer = (Individual * const *)oe_buffer;
	
	AddIndividualsToTable(ind_buffer, ind_count, &treeseq_[0].tables_, &tabled_individuals_hash_, flag);
	
	return gStaticEidosValueVOID;
}

// TREE SEQUENCE RECORDING
//	*********************	- (void)treeSeqOutput(string$ path, [logical$ simplify = T], [logical$ includeModel = T], [No<Dictionary>$ metadata = NULL])
//
EidosValue_SP Species::ExecuteMethod_treeSeqOutput(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *path_value = p_arguments[0].get();
	EidosValue *simplify_value = p_arguments[1].get();
	EidosValue *includeModel_value = p_arguments[2].get();
	EidosValue *metadata_value = p_arguments[3].get();
	
	if (!recording_tree_)
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqOutput): treeSeqOutput() may only be called when tree recording is enabled." << EidosTerminate();
	
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqOutput): treeSeqOutput() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) &&
		(community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) &&
		(community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_treeSeqOutput): treeSeqOutput() may not be called from inside a callback." << EidosTerminate();
	
	std::string path_string = path_value->StringAtIndex_NOCAST(0, nullptr);
	bool simplify = simplify_value->LogicalAtIndex_NOCAST(0, nullptr);
	EidosDictionaryUnretained *metadata_dict = nullptr;
	bool includeModel = includeModel_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	if (metadata_value->Type() == EidosValueType::kValueObject)
		metadata_dict = (EidosDictionaryUnretained *)metadata_value->ObjectElementAtIndex_NOCAST(0, nullptr);
	
	WriteTreeSequence(path_string, simplify, includeModel, metadata_dict);
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)_debug(void)
//
EidosValue_SP Species::ExecuteMethod__debug(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This method is a debugging hook to make it easier to do things on demand during a debugging session.
	// It is not user-visible (e.g., with the methods() method) since it starts with an underscore.
	
	// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
	Eidos_EraseProgress();
	
	//std::unordered_map<slim_objectid_t, std::string> used_subpop_ids_;
	//std::unordered_set<std::string> used_subpop_names_;
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	output_stream << "used_subpop_ids_: " << std::endl;
	for (const auto &element : used_subpop_ids_)
		output_stream << "   " << element.first << " : " << element.second << std::endl;
	
	output_stream << "used_subpop_names_: " << std::endl;
	for (const auto &element : used_subpop_names_)
		output_stream << "   " << element << std::endl;
	
	return gStaticEidosValueVOID;
}


//
//	Species_Class
//
#pragma mark -
#pragma mark Species_Class
#pragma mark -

EidosClass *gSLiM_Species_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Species_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Species_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_avatar,					true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosomes,			true,	kEidosValueMaskObject, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_color,				true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_description,			false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_dimensionality,			true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_periodicity,			true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElementTypes,	true,	kEidosValueMaskObject, gSLiM_GenomicElementType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutations,				true,	kEidosValueMaskObject, gSLiM_Mutation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationTypes,			true,	kEidosValueMaskObject, gSLiM_MutationType_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_name,					true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideBased,		true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_scriptBlocks,			true,	kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sexChromosomes,			true,	kEidosValueMaskObject, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sexEnabled,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulations,			true,	kEidosValueMaskObject, gSLiM_Subpopulation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_substitutions,			true,	kEidosValueMaskObject, gSLiM_Substitution_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_cycle,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Species_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Species_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addPatternForClone, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class))->AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class)->AddArg(kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, "pattern", gEidosDictionaryUnretained_Class)->AddObject_S("parent", gSLiM_Individual_Class)->AddString_OSN("sex", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addPatternForCross, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class))->AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class)->AddArg(kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, "pattern", gEidosDictionaryUnretained_Class)->AddObject_S("parent1", gSLiM_Individual_Class)->AddObject_S("parent2", gSLiM_Individual_Class)->AddString_OSN("sex", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addPatternForNull, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class))->AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class)->AddArg(kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, "pattern", gEidosDictionaryUnretained_Class)->AddString_OSN("sex", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addPatternForRecombinant, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class))->AddArg(kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class)->AddArg(kEidosValueMaskNULL | kEidosValueMaskObject | kEidosValueMaskSingleton, "pattern", gEidosDictionaryUnretained_Class)->AddObject_SN(gStr_strand1, gSLiM_Haplosome_Class)->AddObject_SN(gStr_strand2, gSLiM_Haplosome_Class)->AddInt_N(gStr_breaks1)->AddObject_SN(gStr_strand3, gSLiM_Haplosome_Class)->AddObject_SN(gStr_strand4, gSLiM_Haplosome_Class)->AddInt_N(gStr_breaks2)->AddString_OSN("sex", gStaticEidosValueNULL)->AddLogical_OS("randomizeStrands", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSubpop, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->AddIntString_S("subpopID")->AddInt_S("size")->AddFloat_OS("sexRatio", gStaticEidosValue_Float0Point5)->AddLogical_OS("haploid", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSubpopSplit, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->AddIntString_S("subpopID")->AddInt_S("size")->AddIntObject_S("sourceSubpop", gSLiM_Subpopulation_Class)->AddFloat_OS("sexRatio", gStaticEidosValue_Float0Point5));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_chromosomesOfType, kEidosValueMaskObject, gSLiM_Chromosome_Class))->AddString_S("type"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_chromosomesWithIDs, kEidosValueMaskObject, gSLiM_Chromosome_Class))->AddInt("ids"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_chromosomesWithSymbols, kEidosValueMaskObject, gSLiM_Chromosome_Class))->AddString("symbols"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_individualsWithPedigreeIDs, kEidosValueMaskObject, gSLiM_Individual_Class))->AddInt("pedigreeIDs")->AddIntObject_ON("subpops", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_killIndividuals, kEidosValueMaskVOID))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationCounts, kEidosValueMaskInt))->AddIntObject_N("subpops", gSLiM_Subpopulation_Class)->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationFrequencies, kEidosValueMaskFloat))->AddIntObject_N("subpops", gSLiM_Subpopulation_Class)->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputFixedMutations, kEidosValueMaskVOID))->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("objectTags", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputFull, kEidosValueMaskVOID))->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("binary", gStaticEidosValue_LogicalF)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("spatialPositions", gStaticEidosValue_LogicalT)->AddLogical_OS("ages", gStaticEidosValue_LogicalT)->AddLogical_OS("ancestralNucleotides", gStaticEidosValue_LogicalT)->AddLogical_OS("pedigreeIDs", gStaticEidosValue_LogicalF)->AddLogical_OS("objectTags", gStaticEidosValue_LogicalF)->AddLogical_OS("substitutions", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMutations, kEidosValueMaskVOID))->AddObject("mutations", gSLiM_Mutation_Class)->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("objectTags", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_readFromPopulationFile, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddString_S(gEidosStr_filePath)->AddObject_OSN("subpopMap", gEidosDictionaryUnretained_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_recalculateFitness, kEidosValueMaskVOID))->AddInt_OSN("tick", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerFitnessEffectCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerMateChoiceCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerModifyChildCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerRecombinationCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerSurvivalCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerMutationCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_OSN("mutType", gSLiM_MutationType_Class, gStaticEidosValueNULL)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerMutationEffectCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_S("mutType", gSLiM_MutationType_Class)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_registerReproductionCallback, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SLiMEidosBlock_Class))->AddIntString_SN("id")->AddString_S(gEidosStr_source)->AddIntObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddString_OSN("sex", gStaticEidosValueNULL)->AddInt_OSN("start", gStaticEidosValueNULL)->AddInt_OSN("end", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_simulationFinished, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_skipTick, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_subsetMutations, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddObject_OSN("exclude", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddIntObject_OSN("mutType", gSLiM_MutationType_Class, gStaticEidosValueNULL)->AddInt_OSN("position", gStaticEidosValueNULL)->AddIntString_OSN("nucleotide", gStaticEidosValueNULL)->AddInt_OSN("tag", gStaticEidosValueNULL)->AddInt_OSN("id", gStaticEidosValueNULL)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_substitutionsOfType, kEidosValueMaskObject, gSLiM_Substitution_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqCoalesced, kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqSimplify, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqRememberIndividuals, kEidosValueMaskVOID))->AddObject("individuals", gSLiM_Individual_Class)->AddLogical_OS("permanent", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_treeSeqOutput, kEidosValueMaskVOID))->AddString_S("path")->AddLogical_OS("simplify", gStaticEidosValue_LogicalT)->AddLogical_OS("includeModel", gStaticEidosValue_LogicalT)->AddObject_OSN("metadata", gEidosDictionaryUnretained_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr__debug, kEidosValueMaskVOID)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
