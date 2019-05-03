//
//  slim_global.cpp
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
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


#include "slim_global.h"
#include "mutation.h"
#include "mutation_run.h"
#include "slim_sim.h"
#include "subpopulation.h"
#include "sparse_array.h"

#include <string>
#include <vector>


EidosValue_String_SP gStaticEidosValue_StringA;
EidosValue_String_SP gStaticEidosValue_StringC;
EidosValue_String_SP gStaticEidosValue_StringG;
EidosValue_String_SP gStaticEidosValue_StringT;


void TestSparseArray(void);
void TestSparseArray(void)
{
#if 0
	{
		// This should succeed and contain six elements
		SparseArray sa(5, 5);
		uint32_t row0cols[] = {0, 3, 2};
		double row0dists[] = {0, 3, 2};
		double row0strengths[] = {0.05, 0.35, 0.25};
		uint32_t row1cols[] = {4};
		double row1dists[] = {4};
		double row1strengths[] = {1.45};
		uint32_t row3cols[] = {4, 1};
		double row3dists[] = {4, 1};
		double row3strengths[] = {3.45, 3.15};
		
		sa.AddRowInteractions(0, row0cols, row0dists, row0strengths, 3);
		sa.AddRowInteractions(1, row1cols, row1dists, row1strengths, 1);
		sa.AddRowInteractions(2, nullptr, nullptr, nullptr, 0);
		sa.AddRowInteractions(3, row3cols, row3dists, row3strengths, 2);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif
	
#if 0
	{
		// This should succeed and contain six elements, identical to the previous
		SparseArray sa(5, 5);
		
		sa.AddEntryInteraction(0, 0, 0, 0.05);
		sa.AddEntryInteraction(0, 3, 3, 0.35);
		sa.AddEntryInteraction(0, 2, 2, 0.25);
		sa.AddEntryInteraction(1, 4, 4, 1.45);
		sa.AddEntryInteraction(3, 4, 4, 3.45);
		sa.AddEntryInteraction(3, 1, 1, 3.15);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif

#if 0
	{
		// This should fail because row 1 is added twice
		SparseArray sa(5, 5);
		uint32_t row0cols[] = {0, 3, 2};
		double row0dists[] = {0, 3, 2};
		double row0strengths[] = {0.05, 0.35, 0.25};
		uint32_t row1cols[] = {4};
		double row1dists[] = {4};
		double row1strengths[] = {1.45};
		
		sa.AddRowInteractions(0, row0cols, row0dists, row0strengths, 3);
		sa.AddRowInteractions(1, row1cols, row1dists, row1strengths, 1);
		sa.AddRowInteractions(1, row1cols, row1dists, row1strengths, 1);
	}
#endif
	
#if 0
	{
		// This should fail because row 0 is after row 1
		SparseArray sa(5, 5);
		uint32_t row0cols[] = {0, 3, 2};
		double row0dists[] = {0, 3, 2};
		double row0strengths[] = {0.05, 0.35, 0.25};
		uint32_t row1cols[] = {4};
		double row1dists[] = {4};
		double row1strengths[] = {1.45};
		
		sa.AddRowInteractions(0, nullptr, nullptr, nullptr, 0);
		sa.AddRowInteractions(1, row1cols, row1dists, row1strengths, 1);
		sa.AddRowInteractions(0, row0cols, row0dists, row0strengths, 3);
	}
#endif
	
#if 0
	{
		// This should fail because row 0 is not added first
		SparseArray sa(5, 5);
		uint32_t row1cols[] = {4};
		double row1dists[] = {4};
		double row1strengths[] = {1.45};
		
		sa.AddRowInteractions(1, row1cols, row1dists, row1strengths, 1);
	}
#endif
	
#if 0
	{
		// This should fail because rows are added out of order
		SparseArray sa(5, 5);
		
		sa.AddEntryInteraction(0, 0, 0, 0.05);
		sa.AddEntryInteraction(1, 4, 4, 1.45);
		sa.AddEntryInteraction(0, 3, 3, 0.35);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif
	
#if 0
	{
		// This should fail because a row is added that is beyond bounds
		SparseArray sa(5, 5);
		
		sa.AddEntryInteraction(5, 0, 0, 0.05);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif
	
#if 0
	{
		// This should fail because a column is added that is beyond bounds
		SparseArray sa(5, 5);
		
		sa.AddEntryInteraction(0, 5, 0, 0.05);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif
	
#if 0
	{
		// stress test by creating a large number of sparse arrays by entry and cross-checking them
		for (int trial = 0; trial < 10000; ++trial)
		{
			double *distances = (double *)calloc(100 * 100, sizeof(double));
			double *strengths = (double *)calloc(100 * 100, sizeof(double));
			int n_entries = random() % 5000;
			
			for (int entry = 1; entry <= n_entries; ++entry)
			{
				int entry_index = random() % 10000;
				
				distances[entry_index] = entry;
				strengths[entry_index] = random();
			}
			
			SparseArray sa(100, 100);
			
			for (int row = 0; row < 100; row++)
				for (int col = 0; col < 100; ++col)
					if (*(distances + row + col * 100) != 0)
						sa.AddEntryInteraction(row, col, *(distances + row + col * 100), *(strengths + row + col * 100));
			sa.Finished();
			
			for (int col = 0; col < 100; ++col)
				for (int row = 0; row < 100; row++)
					if (*(distances + row + col * 100) == 0)
					{
						if (!isinf(sa.Distance(row, col)))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): distance defined that should be undefined." << EidosTerminate(nullptr);
						if (sa.Strength(row, col) != 0)
							EIDOS_TERMINATION << "ERROR (TestSparseArray): strength defined that should be undefined." << EidosTerminate(nullptr);
					}
					else
					{
						double distance = sa.Distance(row, col);
						double strength = sa.Strength(row, col);
						
						if (isinf(distance))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): distance undefined that should be defined." << EidosTerminate(nullptr);
						if (strength == 0)
							EIDOS_TERMINATION << "ERROR (TestSparseArray): strength undefined that should be defined." << EidosTerminate(nullptr);
						if (distance != *(distances + row + col * 100))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): distance mismatch." << EidosTerminate(nullptr);
						if (strength != *(strengths + row + col * 100))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): strength mismatch." << EidosTerminate(nullptr);
					}
			
			free(distances);
			free(strengths);
		}
	}
#endif
	
#if 0
	{
		// stress test by creating a large number of sparse arrays by row and cross-checking them
		for (int trial = 0; trial < 10000; ++trial)
		{
			double *distances = (double *)calloc(100 * 100, sizeof(double));
			double *strengths = (double *)calloc(100 * 100, sizeof(double));
			int n_entries = random() % 5000;
			
			for (int entry = 1; entry <= n_entries; ++entry)
			{
				int entry_index = random() % 10000;
				
				distances[entry_index] = entry;
				strengths[entry_index] = random();
			}
			
			SparseArray sa(100, 100);
			
			for (int row = 0; row < 100; row++)
			{
				std::vector<uint32_t> columns;
				std::vector<double> row_distances;
				std::vector<double> row_strengths;
				
				for (int col = 0; col < 100; ++col)
					if (*(distances + row + col * 100) != 0)
					{
						columns.push_back(col);
						row_distances.push_back(*(distances + row + col * 100));
						row_strengths.push_back(*(strengths + row + col * 100));
					}
				
				sa.AddRowInteractions(row, columns.data(), row_distances.data(), row_strengths.data(), (uint32_t)columns.size());
			}
			sa.Finished();
			
			for (int col = 0; col < 100; ++col)
				for (int row = 0; row < 100; row++)
					if (*(distances + row + col * 100) == 0)
					{
						if (!isinf(sa.Distance(row, col)))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): distance defined that should be undefined." << EidosTerminate(nullptr);
						if (sa.Strength(row, col) != 0)
							EIDOS_TERMINATION << "ERROR (TestSparseArray): strength defined that should be undefined." << EidosTerminate(nullptr);
					}
					else
					{
						double distance = sa.Distance(row, col);
						double strength = sa.Strength(row, col);
						
						if (isinf(distance))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): distance undefined that should be defined." << EidosTerminate(nullptr);
						if (strength == 0)
							EIDOS_TERMINATION << "ERROR (TestSparseArray): strength undefined that should be defined." << EidosTerminate(nullptr);
						if (distance != *(distances + row + col * 100))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): distance mismatch." << EidosTerminate(nullptr);
						if (strength != *(strengths + row + col * 100))
							EIDOS_TERMINATION << "ERROR (TestSparseArray): strength mismatch." << EidosTerminate(nullptr);
					}
			
			free(distances);
			free(strengths);
		}
	}
#endif
}

void SLiM_WarmUp(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		// Set up our shared pool for Mutation objects
		SLiM_CreateMutationBlock();
		
		// Configure the Eidos context information
		SLiM_ConfigureContext();
		
		// Register global strings and IDs for SLiM; this is in addition to the globals set up by Eidos
		SLiM_RegisterGlobalStringsAndIDs();
		
		// Allocate global permanents
		gStaticEidosValue_StringA = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_A));
		gStaticEidosValue_StringC = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_C));
		gStaticEidosValue_StringG = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_G));
		gStaticEidosValue_StringT = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_T));
		
#if DO_MEMORY_CHECKS
		// Check for a memory limit and prepare for memory-limit testing
		Eidos_CheckRSSAgainstMax("SLiM_WarmUp()", "This internal check should never fail!");
#endif
		
		// Test sparse arrays; these are not structured as unit tests at the moment
		TestSparseArray();
		
		//std::cout << "sizeof(Mutation) == " << sizeof(Mutation) << std::endl;
	}
}


// a stringstream for SLiM output; see the header for details
std::ostringstream gSLiMOut;


#pragma mark -
#pragma mark Types and max values
#pragma mark -

// Functions for casting from Eidos ints (int64_t) to SLiM int types safely
void SLiM_RaiseGenerationRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaiseGenerationRangeError): value " << p_long_value << " for a generation index or duration is out of range." << EidosTerminate();
}

void SLiM_RaiseAgeRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaiseAgeRangeError): value " << p_long_value << " for an individual age is out of range." << EidosTerminate();
}

void SLiM_RaisePositionRangeError(int64_t p_long_value)
{
	EIDOS_TERMINATION << "ERROR (SLiM_RaisePositionRangeError): value " << p_long_value << " for a chromosome position or length is out of range." << EidosTerminate();
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

SLiMSim &SLiM_GetSimFromInterpreter(EidosInterpreter &p_interpreter)
{
#if DEBUG
	// Use dynamic_cast<> only in DEBUG since it is hella slow
	SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
#else
	SLiMSim *sim = (SLiMSim *)(p_interpreter.Context());
#endif
	
	if (!sim)
		EIDOS_TERMINATION << "ERROR (SLiM_GetSimFromInterpreter): (internal error) the sim is not registered as the context pointer." << EidosTerminate();
	
	return *sim;
}

slim_objectid_t SLiM_ExtractObjectIDFromEidosValue_is(EidosValue *p_value, int p_index, char p_prefix_char)
{
	return (p_value->Type() == EidosValueType::kValueInt) ? SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr)) : SLiMEidosScript::ExtractIDFromStringWithPrefix(p_value->StringAtIndex(p_index, nullptr), p_prefix_char, nullptr);
}

MutationType *SLiM_ExtractMutationTypeFromEidosValue_io(EidosValue *p_value, int p_index, SLiMSim &p_sim, const char *p_method_name)
{
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		auto found_muttype_pair = p_sim.MutationTypes().find(mutation_type_id);
		
		if (found_muttype_pair == p_sim.MutationTypes().end())
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractMutationTypeFromEidosValue_io): " << p_method_name << " mutation type m" << mutation_type_id << " not defined." << EidosTerminate();
		
		return found_muttype_pair->second;
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		return dynamic_cast<MutationType *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		return (MutationType *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
	}
}

GenomicElementType *SLiM_ExtractGenomicElementTypeFromEidosValue_io(EidosValue *p_value, int p_index, SLiMSim &p_sim, const char *p_method_name)
{
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t getype_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		auto found_getype_pair = p_sim.GenomicElementTypes().find(getype_id);
		
		if (found_getype_pair == p_sim.GenomicElementTypes().end())
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractGenomicElementTypeFromEidosValue_io): " << p_method_name << " genomic element type g" << getype_id << " not defined." << EidosTerminate();
		
		return found_getype_pair->second;
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		return dynamic_cast<GenomicElementType *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		return (GenomicElementType *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
	}
}

Subpopulation *SLiM_ExtractSubpopulationFromEidosValue_io(EidosValue *p_value, int p_index, SLiMSim &p_sim, const char *p_method_name)
{
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t source_subpop_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		auto found_subpop_pair = p_sim.ThePopulation().subpops_.find(source_subpop_id);
		
		if (found_subpop_pair == p_sim.ThePopulation().subpops_.end())
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSubpopulationFromEidosValue_io): " << p_method_name << " subpopulation p" << source_subpop_id << " not defined." << EidosTerminate();
		
		return found_subpop_pair->second;
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		return dynamic_cast<Subpopulation *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		return (Subpopulation *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
	}
}

SLiMEidosBlock *SLiM_ExtractSLiMEidosBlockFromEidosValue_io(EidosValue *p_value, int p_index, SLiMSim &p_sim, const char *p_method_name)
{
	if (p_value->Type() == EidosValueType::kValueInt)
	{
		slim_objectid_t block_id = SLiMCastToObjectidTypeOrRaise(p_value->IntAtIndex(p_index, nullptr));
		std::vector<SLiMEidosBlock*> &script_blocks = p_sim.AllScriptBlocks();
		
		for (SLiMEidosBlock *found_block : script_blocks)
			if (found_block->block_id_ == block_id)
				return found_block;
		
		EIDOS_TERMINATION << "ERROR (SLiM_ExtractSLiMEidosBlockFromEidosValue_io): " << p_method_name << " SLiMEidosBlock s" << block_id << " not defined." << EidosTerminate();
	}
	else
	{
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// the class of the object here should be guaranteed by the caller anyway
		return dynamic_cast<SLiMEidosBlock *>(p_value->ObjectElementAtIndex(p_index, nullptr));
#else
		return (SLiMEidosBlock *)(p_value->ObjectElementAtIndex(p_index, nullptr));
#endif
	}
}


#pragma mark -
#pragma mark Shared SLiM types and enumerations
#pragma mark -

// Verbosity, from the command-line option -l[ong]
bool SLiM_verbose_output = false;


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


#pragma mark -
#pragma mark NucleotideArray
#pragma mark -

NucleotideArray::NucleotideArray(std::size_t p_length, const int64_t *p_int_buffer) : length_(p_length)
{
	buffer_ = (uint64_t *)malloc(((length_ + 31) / 32) * sizeof(uint64_t));
	
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
				
				throw std::out_of_range("integer nucleotide value out of range");
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
		nuc_lookup = (uint8_t *)malloc(256 * sizeof(uint8_t));
		
		for (int i = 0; i < 256; ++i)
			nuc_lookup[i] = 4;	// placeholder illegal value
		
		nuc_lookup['A'] = 0;
		nuc_lookup['C'] = 1;
		nuc_lookup['G'] = 2;
		nuc_lookup['T'] = 3;
	}
	
	return nuc_lookup;
}

NucleotideArray::NucleotideArray(std::size_t p_length, const char *p_char_buffer) : length_(p_length)
{
	uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
	
	buffer_ = (uint64_t *)malloc(((length_ + 31) / 32) * sizeof(uint64_t));
	
	// Eat 32 nucleotides at a time if we can
	std::size_t index = 0, buf_index = 0;
	
	for ( ; index < length_; index += 32)
	{
		uint64_t accumulator = 0;
		
		for (std::size_t i = 0; i < 32; )
		{
			char nuc_char = p_char_buffer[index + i];
			uint64_t nuc = nuc_lookup[nuc_char];
			
			if (nuc > 3)
			{
				free(buffer_);
				buffer_ = nullptr;
				
				throw std::out_of_range("char nucleotide value out of range");
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
				
				throw std::out_of_range("string nucleotide value out of range");
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
				default:	string_result->PushString("*"); break;		// should never happen
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
		}
	}
	else
	{
		// return a singleton string for the whole sequence, "TATA"; we munge the std::string inside the EidosValue to avoid memory copying, very naughty
		static const char nuc_chars[4] = {'A', 'C', 'G', 'T'};
		EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		std::string &nuc_string = string_result->StringValue_Mutable();
		
		nuc_string.resize(length);	// create space for all the nucleotides we will generate
		
		char *nuc_string_ptr = &nuc_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
		
		for (int value_index = 0; value_index < length; ++value_index)
			nuc_string_ptr[value_index] = nuc_chars[NucleotideAtIndex(start + value_index)];
		
		return EidosValue_SP(string_result);
	}
	
	return gStaticEidosValueNULL;
}

void NucleotideArray::WriteNucleotidesToBuffer(char *buffer) const
{
	static const char nuc_chars[4] = {'A', 'C', 'G', 'T'};
	
	for (std::size_t index = 0; index < length_; ++index)
		buffer[index] = nuc_chars[NucleotideAtIndex(index)];
}

void NucleotideArray::ReadNucleotidesFromBuffer(char *buffer)
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
	static const char nuc_chars[4] = {'A', 'C', 'G', 'T'};
	std::size_t index = 0;
	std::string nuc_string;
	
	// Emit lines of length 70 first; presumably buffering in a string is faster than emitting one character at a time to the stream...
	nuc_string.resize(70);
	
	while (index + 70 <= p_nuc_array.length_)
	{
		for (int line_index = 0; line_index < 70; ++line_index)
			nuc_string[line_index] = nuc_chars[p_nuc_array.NucleotideAtIndex(index + line_index)];
		
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

// initialize...() functions defined by SLiMSim
const std::string gStr_initializeAncestralNucleotides = "initializeAncestralNucleotides";
const std::string gStr_initializeGenomicElement = "initializeGenomicElement";
const std::string gStr_initializeGenomicElementType = "initializeGenomicElementType";
const std::string gStr_initializeMutationType = "initializeMutationType";
const std::string gStr_initializeMutationTypeNuc = "initializeMutationTypeNuc";
const std::string gStr_initializeGeneConversion = "initializeGeneConversion";
const std::string gStr_initializeMutationRate = "initializeMutationRate";
const std::string gStr_initializeHotspotMap = "initializeHotspotMap";
const std::string gStr_initializeRecombinationRate = "initializeRecombinationRate";
const std::string gStr_initializeSex = "initializeSex";
const std::string gStr_initializeSLiMOptions = "initializeSLiMOptions";
const std::string gStr_initializeTreeSeq = "initializeTreeSeq";
const std::string gStr_initializeSLiMModelType = "initializeSLiMModelType";
const std::string gStr_initializeInteractionType = "initializeInteractionType";

// SLiMEidosDictionary
const std::string gStr_getValue = "getValue";
const std::string gStr_setValue = "setValue";

// mostly property names
const std::string gStr_genomicElements = "genomicElements";
const std::string gStr_lastPosition = "lastPosition";
const std::string gStr_hotspotEndPositions = "hotspotEndPositions";
const std::string gStr_hotspotEndPositionsM = "hotspotEndPositionsM";
const std::string gStr_hotspotEndPositionsF = "hotspotEndPositionsF";
const std::string gStr_hotspotMultipliers = "hotspotMultipliers";
const std::string gStr_hotspotMultipliersM = "hotspotMultipliersM";
const std::string gStr_hotspotMultipliersF = "hotspotMultipliersF";
const std::string gStr_mutationEndPositions = "mutationEndPositions";
const std::string gStr_mutationEndPositionsM = "mutationEndPositionsM";
const std::string gStr_mutationEndPositionsF = "mutationEndPositionsF";
const std::string gStr_mutationRates = "mutationRates";
const std::string gStr_mutationRatesM = "mutationRatesM";
const std::string gStr_mutationRatesF = "mutationRatesF";
const std::string gStr_overallMutationRate = "overallMutationRate";
const std::string gStr_overallMutationRateM = "overallMutationRateM";
const std::string gStr_overallMutationRateF = "overallMutationRateF";
const std::string gStr_overallRecombinationRate = "overallRecombinationRate";
const std::string gStr_overallRecombinationRateM = "overallRecombinationRateM";
const std::string gStr_overallRecombinationRateF = "overallRecombinationRateF";
const std::string gStr_recombinationEndPositions = "recombinationEndPositions";
const std::string gStr_recombinationEndPositionsM = "recombinationEndPositionsM";
const std::string gStr_recombinationEndPositionsF = "recombinationEndPositionsF";
const std::string gStr_recombinationRates = "recombinationRates";
const std::string gStr_recombinationRatesM = "recombinationRatesM";
const std::string gStr_recombinationRatesF = "recombinationRatesF";
const std::string gStr_geneConversionEnabled = "geneConversionEnabled";
const std::string gStr_geneConversionGCBias = "geneConversionGCBias";
const std::string gStr_geneConversionNonCrossoverFraction = "geneConversionNonCrossoverFraction";
const std::string gStr_geneConversionMeanLength = "geneConversionMeanLength";
const std::string gStr_geneConversionSimpleConversionFraction = "geneConversionSimpleConversionFraction";
const std::string gStr_genomeType = "genomeType";
const std::string gStr_isNullGenome = "isNullGenome";
const std::string gStr_mutations = "mutations";
const std::string gStr_uniqueMutations = "uniqueMutations";
const std::string gStr_genomicElementType = "genomicElementType";
const std::string gStr_startPosition = "startPosition";
const std::string gStr_endPosition = "endPosition";
const std::string gStr_id = "id";
const std::string gStr_mutationTypes = "mutationTypes";
const std::string gStr_mutationFractions = "mutationFractions";
const std::string gStr_mutationMatrix = "mutationMatrix";
const std::string gStr_mutationType = "mutationType";
const std::string gStr_nucleotide = "nucleotide";
const std::string gStr_nucleotideValue = "nucleotideValue";
const std::string gStr_originGeneration = "originGeneration";
const std::string gStr_position = "position";
const std::string gStr_selectionCoeff = "selectionCoeff";
const std::string gStr_subpopID = "subpopID";
const std::string gStr_convertToSubstitution = "convertToSubstitution";
const std::string gStr_distributionType = "distributionType";
const std::string gStr_distributionParams = "distributionParams";
const std::string gStr_dominanceCoeff = "dominanceCoeff";
const std::string gStr_mutationStackGroup = "mutationStackGroup";
const std::string gStr_mutationStackPolicy = "mutationStackPolicy";
//const std::string gStr_start = "start";
//const std::string gStr_end = "end";
//const std::string gStr_type = "type";
//const std::string gStr_source = "source";
const std::string gStr_active = "active";
const std::string gStr_chromosome = "chromosome";
const std::string gStr_chromosomeType = "chromosomeType";
const std::string gStr_genomicElementTypes = "genomicElementTypes";
const std::string gStr_inSLiMgui = "inSLiMgui";
const std::string gStr_interactionTypes = "interactionTypes";
const std::string gStr_modelType = "modelType";
const std::string gStr_nucleotideBased = "nucleotideBased";
const std::string gStr_scriptBlocks = "scriptBlocks";
const std::string gStr_sexEnabled = "sexEnabled";
const std::string gStr_subpopulations = "subpopulations";
const std::string gStr_substitutions = "substitutions";
const std::string gStr_dominanceCoeffX = "dominanceCoeffX";
const std::string gStr_generation = "generation";
const std::string gStr_colorSubstitution = "colorSubstitution";
const std::string gStr_tag = "tag";
const std::string gStr_tagF = "tagF";
const std::string gStr_migrant = "migrant";
const std::string gStr_fitnessScaling = "fitnessScaling";
const std::string gStr_firstMaleIndex = "firstMaleIndex";
const std::string gStr_genomes = "genomes";
const std::string gStr_sex = "sex";
const std::string gStr_individuals = "individuals";
const std::string gStr_subpopulation = "subpopulation";
const std::string gStr_index = "index";
const std::string gStr_immigrantSubpopIDs = "immigrantSubpopIDs";
const std::string gStr_immigrantSubpopFractions = "immigrantSubpopFractions";
const std::string gStr_selfingRate = "selfingRate";
const std::string gStr_cloningRate = "cloningRate";
const std::string gStr_sexRatio = "sexRatio";
const std::string gStr_spatialBounds = "spatialBounds";
const std::string gStr_individualCount = "individualCount";
const std::string gStr_fixationGeneration = "fixationGeneration";
const std::string gStr_age = "age";
const std::string gStr_pedigreeID = "pedigreeID";
const std::string gStr_pedigreeParentIDs = "pedigreeParentIDs";
const std::string gStr_pedigreeGrandparentIDs = "pedigreeGrandparentIDs";
const std::string gStr_genomePedigreeID = "genomePedigreeID";
const std::string gStr_reciprocal = "reciprocal";
const std::string gStr_sexSegregation = "sexSegregation";
const std::string gStr_dimensionality = "dimensionality";
const std::string gStr_periodicity = "periodicity";
const std::string gStr_spatiality = "spatiality";
const std::string gStr_spatialPosition = "spatialPosition";
const std::string gStr_maxDistance = "maxDistance";

// mostly method names
const std::string gStr_ancestralNucleotides = "ancestralNucleotides";
const std::string gStr_nucleotides = "nucleotides";
const std::string gStr_setAncestralNucleotides = "setAncestralNucleotides";
const std::string gStr_setGeneConversion = "setGeneConversion";
const std::string gStr_setHotspotMap = "setHotspotMap";
const std::string gStr_setMutationRate = "setMutationRate";
const std::string gStr_setRecombinationRate = "setRecombinationRate";
const std::string gStr_drawBreakpoints = "drawBreakpoints";
const std::string gStr_addMutations = "addMutations";
const std::string gStr_addNewDrawnMutation = "addNewDrawnMutation";
const std::string gStr_addNewMutation = "addNewMutation";
const std::string gStr_containsMutations = "containsMutations";
const std::string gStr_countOfMutationsOfType = "countOfMutationsOfType";
const std::string gStr_positionsOfMutationsOfType = "positionsOfMutationsOfType";
const std::string gStr_containsMarkerMutation = "containsMarkerMutation";
const std::string gStr_relatedness = "relatedness";
const std::string gStr_mutationsOfType = "mutationsOfType";
const std::string gStr_setSpatialPosition = "setSpatialPosition";
const std::string gStr_sumOfMutationsOfType = "sumOfMutationsOfType";
const std::string gStr_uniqueMutationsOfType = "uniqueMutationsOfType";
const std::string gStr_readFromMS = "readFromMS";
const std::string gStr_readFromVCF = "readFromVCF";
const std::string gStr_removeMutations = "removeMutations";
const std::string gStr_setGenomicElementType = "setGenomicElementType";
const std::string gStr_setMutationFractions = "setMutationFractions";
const std::string gStr_setMutationMatrix = "setMutationMatrix";
const std::string gStr_setSelectionCoeff = "setSelectionCoeff";
const std::string gStr_setMutationType = "setMutationType";
const std::string gStr_drawSelectionCoefficient = "drawSelectionCoefficient";
const std::string gStr_setDistribution = "setDistribution";
const std::string gStr_addSubpop = "addSubpop";
const std::string gStr_addSubpopSplit = "addSubpopSplit";
const std::string gStr_deregisterScriptBlock = "deregisterScriptBlock";
const std::string gStr_mutationFrequencies = "mutationFrequencies";
const std::string gStr_mutationCounts = "mutationCounts";
//const std::string gStr_mutationsOfType = "mutationsOfType";
//const std::string gStr_countOfMutationsOfType = "countOfMutationsOfType";
const std::string gStr_outputFixedMutations = "outputFixedMutations";
const std::string gStr_outputFull = "outputFull";
const std::string gStr_outputMutations = "outputMutations";
const std::string gStr_outputUsage = "outputUsage";
const std::string gStr_readFromPopulationFile = "readFromPopulationFile";
const std::string gStr_recalculateFitness = "recalculateFitness";
const std::string gStr_registerEarlyEvent = "registerEarlyEvent";
const std::string gStr_registerLateEvent = "registerLateEvent";
const std::string gStr_registerFitnessCallback = "registerFitnessCallback";
const std::string gStr_registerInteractionCallback = "registerInteractionCallback";
const std::string gStr_registerMateChoiceCallback = "registerMateChoiceCallback";
const std::string gStr_registerModifyChildCallback = "registerModifyChildCallback";
const std::string gStr_registerRecombinationCallback = "registerRecombinationCallback";
const std::string gStr_registerMutationCallback = "registerMutationCallback";
const std::string gStr_registerReproductionCallback = "registerReproductionCallback";
const std::string gStr_rescheduleScriptBlock = "rescheduleScriptBlock";
const std::string gStr_simulationFinished = "simulationFinished";
const std::string gStr_treeSeqCoalesced = "treeSeqCoalesced";
const std::string gStr_treeSeqSimplify = "treeSeqSimplify";
const std::string gStr_treeSeqRememberIndividuals = "treeSeqRememberIndividuals";
const std::string gStr_treeSeqOutput = "treeSeqOutput";
const std::string gStr_setMigrationRates = "setMigrationRates";
const std::string gStr_pointInBounds = "pointInBounds";
const std::string gStr_pointReflected = "pointReflected";
const std::string gStr_pointStopped = "pointStopped";
const std::string gStr_pointPeriodic = "pointPeriodic";
const std::string gStr_pointUniform = "pointUniform";
const std::string gStr_setCloningRate = "setCloningRate";
const std::string gStr_setSelfingRate = "setSelfingRate";
const std::string gStr_setSexRatio = "setSexRatio";
const std::string gStr_setSpatialBounds = "setSpatialBounds";
const std::string gStr_setSubpopulationSize = "setSubpopulationSize";
const std::string gStr_addCloned = "addCloned";
const std::string gStr_addCrossed = "addCrossed";
const std::string gStr_addEmpty = "addEmpty";
const std::string gStr_addRecombinant = "addRecombinant";
const std::string gStr_addSelfed = "addSelfed";
const std::string gStr_takeMigrants = "takeMigrants";
const std::string gStr_removeSubpopulation = "removeSubpopulation";
const std::string gStr_cachedFitness = "cachedFitness";
const std::string gStr_sampleIndividuals = "sampleIndividuals";
const std::string gStr_subsetIndividuals = "subsetIndividuals";
const std::string gStr_defineSpatialMap = "defineSpatialMap";
const std::string gStr_spatialMapColor = "spatialMapColor";
const std::string gStr_spatialMapValue = "spatialMapValue";
const std::string gStr_outputMSSample = "outputMSSample";
const std::string gStr_outputVCFSample = "outputVCFSample";
const std::string gStr_outputSample = "outputSample";
const std::string gStr_outputMS = "outputMS";
const std::string gStr_outputVCF = "outputVCF";
const std::string gStr_output = "output";
const std::string gStr_evaluate = "evaluate";
const std::string gStr_distance = "distance";
const std::string gStr_interactionDistance = "interactionDistance";
const std::string gStr_distanceToPoint = "distanceToPoint";
const std::string gStr_nearestNeighbors = "nearestNeighbors";
const std::string gStr_nearestInteractingNeighbors = "nearestInteractingNeighbors";
const std::string gStr_interactingNeighborCount = "interactingNeighborCount";
const std::string gStr_nearestNeighborsOfPoint = "nearestNeighborsOfPoint";
const std::string gStr_setInteractionFunction = "setInteractionFunction";
const std::string gStr_strength = "strength";
const std::string gStr_totalOfNeighborStrengths = "totalOfNeighborStrengths";
const std::string gStr_unevaluate = "unevaluate";
const std::string gStr_drawByStrength = "drawByStrength";

// mostly SLiM variable names used in callbacks and such
const std::string gStr_sim = "sim";
const std::string gStr_self = "self";
const std::string gStr_individual = "individual";
const std::string gStr_element = "element";
const std::string gStr_genome = "genome";
const std::string gStr_genome1 = "genome1";
const std::string gStr_genome2 = "genome2";
const std::string gStr_subpop = "subpop";
const std::string gStr_sourceSubpop = "sourceSubpop";
//const std::string gStr_weights = "weights";		now gEidosStr_weights
const std::string gStr_child = "child";
const std::string gStr_childGenome1 = "childGenome1";
const std::string gStr_childGenome2 = "childGenome2";
const std::string gStr_childIsFemale = "childIsFemale";
const std::string gStr_parent = "parent";
const std::string gStr_parent1 = "parent1";
const std::string gStr_parent1Genome1 = "parent1Genome1";
const std::string gStr_parent1Genome2 = "parent1Genome2";
const std::string gStr_isCloning = "isCloning";
const std::string gStr_isSelfing = "isSelfing";
const std::string gStr_parent2 = "parent2";
const std::string gStr_parent2Genome1 = "parent2Genome1";
const std::string gStr_parent2Genome2 = "parent2Genome2";
const std::string gStr_mut = "mut";
const std::string gStr_relFitness = "relFitness";
const std::string gStr_homozygous = "homozygous";
const std::string gStr_breakpoints = "breakpoints";
const std::string gStr_receiver = "receiver";
const std::string gStr_exerter = "exerter";
const std::string gStr_originalNuc = "originalNuc";

// SLiMgui instance name and methods
const std::string gStr_slimgui = "slimgui";
const std::string gStr_pid = "pid";
const std::string gStr_openDocument = "openDocument";
const std::string gStr_pauseExecution = "pauseExecution";
const std::string gStr_configureDisplay = "configureDisplay";

// mostly SLiM element types
const std::string gStr_SLiMEidosDictionary = "SLiMEidosDictionary";
const std::string gStr_Chromosome = "Chromosome";
//const std::string gStr_Genome = "Genome";				// in Eidos; see EidosValue_Object::EidosValue_Object()
const std::string gStr_GenomicElement = "GenomicElement";
const std::string gStr_GenomicElementType = "GenomicElementType";
//const std::string gStr_Mutation = "Mutation";			// in Eidos; see EidosValue_Object::EidosValue_Object()
const std::string gStr_MutationType = "MutationType";
const std::string gStr_SLiMEidosBlock = "SLiMEidosBlock";
const std::string gStr_SLiMSim = "SLiMSim";
const std::string gStr_Subpopulation = "Subpopulation";
//const std::string gStr_Individual = "Individual";		// in Eidos; see EidosValue_Object::EidosValue_Object()
const std::string gStr_Substitution = "Substitution";
const std::string gStr_InteractionType = "InteractionType";
const std::string gStr_SLiMgui = "SLiMgui";

// mostly other fixed strings
const std::string gStr_A = "A";
const std::string gStr_C = "C";
const std::string gStr_G = "G";
const std::string gStr_T = "T";
const std::string gStr_X = "X";
const std::string gStr_Y = "Y";
const std::string gStr_f = "f";
const std::string gStr_g = "g";
const std::string gStr_e = "e";
//const std::string gStr_n = "n";		now gEidosStr_n
const std::string gStr_w = "w";
const std::string gStr_l = "l";
//const std::string gStr_s = "s";		now gEidosStr_s
const std::string gStr_early = "early";
const std::string gStr_late = "late";
const std::string gStr_initialize = "initialize";
const std::string gStr_fitness = "fitness";
const std::string gStr_interaction = "interaction";
const std::string gStr_mateChoice = "mateChoice";
const std::string gStr_modifyChild = "modifyChild";
const std::string gStr_recombination = "recombination";
const std::string gStr_mutation = "mutation";
const std::string gStr_reproduction = "reproduction";


void SLiM_ConfigureContext(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		gEidosContextVersion = SLIM_VERSION_FLOAT;
		gEidosContextVersionString = std::string("SLiM version ") + std::string(SLIM_VERSION_STRING);
		gEidosContextLicense = "SLiM is free software: you can redistribute it and/or\nmodify it under the terms of the GNU General Public\nLicense as published by the Free Software Foundation,\neither version 3 of the License, or (at your option)\nany later version.\n\nSLiM is distributed in the hope that it will be\nuseful, but WITHOUT ANY WARRANTY; without even the\nimplied warranty of MERCHANTABILITY or FITNESS FOR\nA PARTICULAR PURPOSE.  See the GNU General Public\nLicense for more details.\n\nYou should have received a copy of the GNU General\nPublic License along with SLiM.  If not, see\n<http://www.gnu.org/licenses/>.\n";
		gEidosContextCitation = "To cite SLiM in publications please use:\n\nHaller, B.C., and Messer, P.W. (2019). SLiM 3: Forward\ngenetic simulations beyond the Wright-Fisher model.\nMolecular Biology and Evolution 36(3), 632-637.\nDOI: http://dx.doi.org/10.1093/molbev/msy228\n\nFor papers using tree-sequence recording, please cite:\n\nHaller, B.C., Galloway, J., Kelleher, J., Messer, P.W.,\n& Ralph, P.L. (2019). Tree‐sequence recording in SLiM\nopens new horizons for forward‐time simulation of whole\ngenomes. Molecular Ecology Resources 19(2), 552-566.\nDOI: https://doi.org/10.1111/1755-0998.12968\n";
		
		gEidosContextClasses.push_back(gSLiM_Chromosome_Class);
		gEidosContextClasses.push_back(gSLiM_Genome_Class);
		gEidosContextClasses.push_back(gSLiM_GenomicElement_Class);
		gEidosContextClasses.push_back(gSLiM_GenomicElementType_Class);
		gEidosContextClasses.push_back(gSLiM_Individual_Class);
		gEidosContextClasses.push_back(gSLiM_InteractionType_Class);
		gEidosContextClasses.push_back(gSLiM_Mutation_Class);
		gEidosContextClasses.push_back(gSLiM_MutationType_Class);
		gEidosContextClasses.push_back(gSLiM_SLiMEidosBlock_Class);
		gEidosContextClasses.push_back(gSLiM_SLiMSim_Class);
		gEidosContextClasses.push_back(gSLiM_Subpopulation_Class);
		gEidosContextClasses.push_back(gSLiM_Substitution_Class);
	}
}

void SLiM_RegisterGlobalStringsAndIDs(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		Eidos_RegisterStringForGlobalID(gStr_initializeAncestralNucleotides, gID_initializeAncestralNucleotides);
		Eidos_RegisterStringForGlobalID(gStr_initializeGenomicElement, gID_initializeGenomicElement);
		Eidos_RegisterStringForGlobalID(gStr_initializeGenomicElementType, gID_initializeGenomicElementType);
		Eidos_RegisterStringForGlobalID(gStr_initializeMutationType, gID_initializeMutationType);
		Eidos_RegisterStringForGlobalID(gStr_initializeMutationTypeNuc, gID_initializeMutationTypeNuc);
		Eidos_RegisterStringForGlobalID(gStr_initializeGeneConversion, gID_initializeGeneConversion);
		Eidos_RegisterStringForGlobalID(gStr_initializeMutationRate, gID_initializeMutationRate);
		Eidos_RegisterStringForGlobalID(gStr_initializeHotspotMap, gID_initializeHotspotMap);
		Eidos_RegisterStringForGlobalID(gStr_initializeRecombinationRate, gID_initializeRecombinationRate);
		Eidos_RegisterStringForGlobalID(gStr_initializeSex, gID_initializeSex);
		Eidos_RegisterStringForGlobalID(gStr_initializeSLiMOptions, gID_initializeSLiMOptions);
		Eidos_RegisterStringForGlobalID(gStr_initializeTreeSeq, gID_initializeTreeSeq);
		Eidos_RegisterStringForGlobalID(gStr_initializeSLiMModelType, gID_initializeSLiMModelType);
		Eidos_RegisterStringForGlobalID(gStr_initializeInteractionType, gID_initializeInteractionType);
		
		Eidos_RegisterStringForGlobalID(gStr_getValue, gID_getValue);
		Eidos_RegisterStringForGlobalID(gStr_setValue, gID_setValue);
		
		Eidos_RegisterStringForGlobalID(gStr_genomicElements, gID_genomicElements);
		Eidos_RegisterStringForGlobalID(gStr_lastPosition, gID_lastPosition);
		Eidos_RegisterStringForGlobalID(gStr_hotspotEndPositions, gID_hotspotEndPositions);
		Eidos_RegisterStringForGlobalID(gStr_hotspotEndPositionsM, gID_hotspotEndPositionsM);
		Eidos_RegisterStringForGlobalID(gStr_hotspotEndPositionsF, gID_hotspotEndPositionsF);
		Eidos_RegisterStringForGlobalID(gStr_hotspotMultipliers, gID_hotspotMultipliers);
		Eidos_RegisterStringForGlobalID(gStr_hotspotMultipliersM, gID_hotspotMultipliersM);
		Eidos_RegisterStringForGlobalID(gStr_hotspotMultipliersF, gID_hotspotMultipliersF);
		Eidos_RegisterStringForGlobalID(gStr_mutationEndPositions, gID_mutationEndPositions);
		Eidos_RegisterStringForGlobalID(gStr_mutationEndPositionsM, gID_mutationEndPositionsM);
		Eidos_RegisterStringForGlobalID(gStr_mutationEndPositionsF, gID_mutationEndPositionsF);
		Eidos_RegisterStringForGlobalID(gStr_mutationRates, gID_mutationRates);
		Eidos_RegisterStringForGlobalID(gStr_mutationRatesM, gID_mutationRatesM);
		Eidos_RegisterStringForGlobalID(gStr_mutationRatesF, gID_mutationRatesF);
		Eidos_RegisterStringForGlobalID(gStr_overallMutationRate, gID_overallMutationRate);
		Eidos_RegisterStringForGlobalID(gStr_overallMutationRateM, gID_overallMutationRateM);
		Eidos_RegisterStringForGlobalID(gStr_overallMutationRateF, gID_overallMutationRateF);
		Eidos_RegisterStringForGlobalID(gStr_overallRecombinationRate, gID_overallRecombinationRate);
		Eidos_RegisterStringForGlobalID(gStr_overallRecombinationRateM, gID_overallRecombinationRateM);
		Eidos_RegisterStringForGlobalID(gStr_overallRecombinationRateF, gID_overallRecombinationRateF);
		Eidos_RegisterStringForGlobalID(gStr_recombinationEndPositions, gID_recombinationEndPositions);
		Eidos_RegisterStringForGlobalID(gStr_recombinationEndPositionsM, gID_recombinationEndPositionsM);
		Eidos_RegisterStringForGlobalID(gStr_recombinationEndPositionsF, gID_recombinationEndPositionsF);
		Eidos_RegisterStringForGlobalID(gStr_recombinationRates, gID_recombinationRates);
		Eidos_RegisterStringForGlobalID(gStr_recombinationRatesM, gID_recombinationRatesM);
		Eidos_RegisterStringForGlobalID(gStr_recombinationRatesF, gID_recombinationRatesF);
		Eidos_RegisterStringForGlobalID(gStr_geneConversionEnabled, gID_geneConversionEnabled);
		Eidos_RegisterStringForGlobalID(gStr_geneConversionGCBias, gID_geneConversionGCBias);
		Eidos_RegisterStringForGlobalID(gStr_geneConversionNonCrossoverFraction, gID_geneConversionNonCrossoverFraction);
		Eidos_RegisterStringForGlobalID(gStr_geneConversionMeanLength, gID_geneConversionMeanLength);
		Eidos_RegisterStringForGlobalID(gStr_geneConversionSimpleConversionFraction, gID_geneConversionSimpleConversionFraction);
		Eidos_RegisterStringForGlobalID(gStr_genomeType, gID_genomeType);
		Eidos_RegisterStringForGlobalID(gStr_isNullGenome, gID_isNullGenome);
		Eidos_RegisterStringForGlobalID(gStr_mutations, gID_mutations);
		Eidos_RegisterStringForGlobalID(gStr_uniqueMutations, gID_uniqueMutations);
		Eidos_RegisterStringForGlobalID(gStr_genomicElementType, gID_genomicElementType);
		Eidos_RegisterStringForGlobalID(gStr_startPosition, gID_startPosition);
		Eidos_RegisterStringForGlobalID(gStr_endPosition, gID_endPosition);
		Eidos_RegisterStringForGlobalID(gStr_id, gID_id);
		Eidos_RegisterStringForGlobalID(gStr_mutationTypes, gID_mutationTypes);
		Eidos_RegisterStringForGlobalID(gStr_mutationFractions, gID_mutationFractions);
		Eidos_RegisterStringForGlobalID(gStr_mutationMatrix, gID_mutationMatrix);
		Eidos_RegisterStringForGlobalID(gStr_mutationType, gID_mutationType);
		Eidos_RegisterStringForGlobalID(gStr_nucleotide, gID_nucleotide);
		Eidos_RegisterStringForGlobalID(gStr_nucleotideValue, gID_nucleotideValue);
		Eidos_RegisterStringForGlobalID(gStr_originGeneration, gID_originGeneration);
		Eidos_RegisterStringForGlobalID(gStr_position, gID_position);
		Eidos_RegisterStringForGlobalID(gStr_selectionCoeff, gID_selectionCoeff);
		Eidos_RegisterStringForGlobalID(gStr_subpopID, gID_subpopID);
		Eidos_RegisterStringForGlobalID(gStr_convertToSubstitution, gID_convertToSubstitution);
		Eidos_RegisterStringForGlobalID(gStr_distributionType, gID_distributionType);
		Eidos_RegisterStringForGlobalID(gStr_distributionParams, gID_distributionParams);
		Eidos_RegisterStringForGlobalID(gStr_dominanceCoeff, gID_dominanceCoeff);
		Eidos_RegisterStringForGlobalID(gStr_mutationStackGroup, gID_mutationStackGroup);
		Eidos_RegisterStringForGlobalID(gStr_mutationStackPolicy, gID_mutationStackPolicy);
		//Eidos_RegisterStringForGlobalID(gStr_start, gID_start);
		//Eidos_RegisterStringForGlobalID(gStr_end, gID_end);
		//Eidos_RegisterStringForGlobalID(gStr_type, gID_type);
		//Eidos_RegisterStringForGlobalID(gStr_source, gID_source);
		Eidos_RegisterStringForGlobalID(gStr_active, gID_active);
		Eidos_RegisterStringForGlobalID(gStr_chromosome, gID_chromosome);
		Eidos_RegisterStringForGlobalID(gStr_chromosomeType, gID_chromosomeType);
		Eidos_RegisterStringForGlobalID(gStr_genomicElementTypes, gID_genomicElementTypes);
		Eidos_RegisterStringForGlobalID(gStr_inSLiMgui, gID_inSLiMgui);
		Eidos_RegisterStringForGlobalID(gStr_interactionTypes, gID_interactionTypes);
		Eidos_RegisterStringForGlobalID(gStr_modelType, gID_modelType);
		Eidos_RegisterStringForGlobalID(gStr_nucleotideBased, gID_nucleotideBased);
		Eidos_RegisterStringForGlobalID(gStr_scriptBlocks, gID_scriptBlocks);
		Eidos_RegisterStringForGlobalID(gStr_sexEnabled, gID_sexEnabled);
		Eidos_RegisterStringForGlobalID(gStr_subpopulations, gID_subpopulations);
		Eidos_RegisterStringForGlobalID(gStr_substitutions, gID_substitutions);
		Eidos_RegisterStringForGlobalID(gStr_dominanceCoeffX, gID_dominanceCoeffX);
		Eidos_RegisterStringForGlobalID(gStr_generation, gID_generation);
		Eidos_RegisterStringForGlobalID(gStr_colorSubstitution, gID_colorSubstitution);
		Eidos_RegisterStringForGlobalID(gStr_tag, gID_tag);
		Eidos_RegisterStringForGlobalID(gStr_tagF, gID_tagF);
		Eidos_RegisterStringForGlobalID(gStr_migrant, gID_migrant);
		Eidos_RegisterStringForGlobalID(gStr_fitnessScaling, gID_fitnessScaling);
		Eidos_RegisterStringForGlobalID(gStr_firstMaleIndex, gID_firstMaleIndex);
		Eidos_RegisterStringForGlobalID(gStr_genomes, gID_genomes);
		Eidos_RegisterStringForGlobalID(gStr_sex, gID_sex);
		Eidos_RegisterStringForGlobalID(gStr_individuals, gID_individuals);
		Eidos_RegisterStringForGlobalID(gStr_subpopulation, gID_subpopulation);
		Eidos_RegisterStringForGlobalID(gStr_index, gID_index);
		Eidos_RegisterStringForGlobalID(gStr_immigrantSubpopIDs, gID_immigrantSubpopIDs);
		Eidos_RegisterStringForGlobalID(gStr_immigrantSubpopFractions, gID_immigrantSubpopFractions);
		Eidos_RegisterStringForGlobalID(gStr_selfingRate, gID_selfingRate);
		Eidos_RegisterStringForGlobalID(gStr_cloningRate, gID_cloningRate);
		Eidos_RegisterStringForGlobalID(gStr_sexRatio, gID_sexRatio);
		Eidos_RegisterStringForGlobalID(gStr_spatialBounds, gID_spatialBounds);
		Eidos_RegisterStringForGlobalID(gStr_individualCount, gID_individualCount);
		Eidos_RegisterStringForGlobalID(gStr_fixationGeneration, gID_fixationGeneration);
		Eidos_RegisterStringForGlobalID(gStr_age, gID_age);
		Eidos_RegisterStringForGlobalID(gStr_pedigreeID, gID_pedigreeID);
		Eidos_RegisterStringForGlobalID(gStr_pedigreeParentIDs, gID_pedigreeParentIDs);
		Eidos_RegisterStringForGlobalID(gStr_pedigreeGrandparentIDs, gID_pedigreeGrandparentIDs);
		Eidos_RegisterStringForGlobalID(gStr_genomePedigreeID, gID_genomePedigreeID);
		Eidos_RegisterStringForGlobalID(gStr_reciprocal, gID_reciprocal);
		Eidos_RegisterStringForGlobalID(gStr_sexSegregation, gID_sexSegregation);
		Eidos_RegisterStringForGlobalID(gStr_dimensionality, gID_dimensionality);
		Eidos_RegisterStringForGlobalID(gStr_periodicity, gID_periodicity);
		Eidos_RegisterStringForGlobalID(gStr_spatiality, gID_spatiality);
		Eidos_RegisterStringForGlobalID(gStr_spatialPosition, gID_spatialPosition);
		Eidos_RegisterStringForGlobalID(gStr_maxDistance, gID_maxDistance);
		
		Eidos_RegisterStringForGlobalID(gStr_ancestralNucleotides, gID_ancestralNucleotides);
		Eidos_RegisterStringForGlobalID(gStr_nucleotides, gID_nucleotides);
		Eidos_RegisterStringForGlobalID(gStr_setAncestralNucleotides, gID_setAncestralNucleotides);
		Eidos_RegisterStringForGlobalID(gStr_setGeneConversion, gID_setGeneConversion);
		Eidos_RegisterStringForGlobalID(gStr_setHotspotMap, gID_setHotspotMap);
		Eidos_RegisterStringForGlobalID(gStr_setMutationRate, gID_setMutationRate);
		Eidos_RegisterStringForGlobalID(gStr_setRecombinationRate, gID_setRecombinationRate);
		Eidos_RegisterStringForGlobalID(gStr_drawBreakpoints, gID_drawBreakpoints);
		Eidos_RegisterStringForGlobalID(gStr_addMutations, gID_addMutations);
		Eidos_RegisterStringForGlobalID(gStr_addNewDrawnMutation, gID_addNewDrawnMutation);
		Eidos_RegisterStringForGlobalID(gStr_addNewMutation, gID_addNewMutation);
		Eidos_RegisterStringForGlobalID(gStr_countOfMutationsOfType, gID_countOfMutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_positionsOfMutationsOfType, gID_positionsOfMutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_containsMarkerMutation, gID_containsMarkerMutation);
		Eidos_RegisterStringForGlobalID(gStr_relatedness, gID_relatedness);
		Eidos_RegisterStringForGlobalID(gStr_containsMutations, gID_containsMutations);
		Eidos_RegisterStringForGlobalID(gStr_mutationsOfType, gID_mutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_setSpatialPosition, gID_setSpatialPosition);
		Eidos_RegisterStringForGlobalID(gStr_sumOfMutationsOfType, gID_sumOfMutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_uniqueMutationsOfType, gID_uniqueMutationsOfType);
		Eidos_RegisterStringForGlobalID(gStr_readFromMS, gID_readFromMS);
		Eidos_RegisterStringForGlobalID(gStr_readFromVCF, gID_readFromVCF);
		Eidos_RegisterStringForGlobalID(gStr_removeMutations, gID_removeMutations);
		Eidos_RegisterStringForGlobalID(gStr_setGenomicElementType, gID_setGenomicElementType);
		Eidos_RegisterStringForGlobalID(gStr_setMutationFractions, gID_setMutationFractions);
		Eidos_RegisterStringForGlobalID(gStr_setMutationMatrix, gID_setMutationMatrix);
		Eidos_RegisterStringForGlobalID(gStr_setSelectionCoeff, gID_setSelectionCoeff);
		Eidos_RegisterStringForGlobalID(gStr_setMutationType, gID_setMutationType);
		Eidos_RegisterStringForGlobalID(gStr_drawSelectionCoefficient, gID_drawSelectionCoefficient);
		Eidos_RegisterStringForGlobalID(gStr_setDistribution, gID_setDistribution);
		Eidos_RegisterStringForGlobalID(gStr_addSubpop, gID_addSubpop);
		Eidos_RegisterStringForGlobalID(gStr_addSubpopSplit, gID_addSubpopSplit);
		Eidos_RegisterStringForGlobalID(gStr_deregisterScriptBlock, gID_deregisterScriptBlock);
		Eidos_RegisterStringForGlobalID(gStr_mutationFrequencies, gID_mutationFrequencies);
		Eidos_RegisterStringForGlobalID(gStr_mutationCounts, gID_mutationCounts);
		Eidos_RegisterStringForGlobalID(gStr_outputFixedMutations, gID_outputFixedMutations);
		Eidos_RegisterStringForGlobalID(gStr_outputFull, gID_outputFull);
		Eidos_RegisterStringForGlobalID(gStr_outputMutations, gID_outputMutations);
		Eidos_RegisterStringForGlobalID(gStr_outputUsage, gID_outputUsage);
		Eidos_RegisterStringForGlobalID(gStr_readFromPopulationFile, gID_readFromPopulationFile);
		Eidos_RegisterStringForGlobalID(gStr_recalculateFitness, gID_recalculateFitness);
		Eidos_RegisterStringForGlobalID(gStr_registerEarlyEvent, gID_registerEarlyEvent);
		Eidos_RegisterStringForGlobalID(gStr_registerLateEvent, gID_registerLateEvent);
		Eidos_RegisterStringForGlobalID(gStr_registerFitnessCallback, gID_registerFitnessCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerInteractionCallback, gID_registerInteractionCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerMateChoiceCallback, gID_registerMateChoiceCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerModifyChildCallback, gID_registerModifyChildCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerRecombinationCallback, gID_registerRecombinationCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerMutationCallback, gID_registerMutationCallback);
		Eidos_RegisterStringForGlobalID(gStr_registerReproductionCallback, gID_registerReproductionCallback);
		Eidos_RegisterStringForGlobalID(gStr_rescheduleScriptBlock, gID_rescheduleScriptBlock);
		Eidos_RegisterStringForGlobalID(gStr_simulationFinished, gID_simulationFinished);
		Eidos_RegisterStringForGlobalID(gStr_treeSeqCoalesced, gID_treeSeqCoalesced);
		Eidos_RegisterStringForGlobalID(gStr_treeSeqSimplify, gID_treeSeqSimplify);
		Eidos_RegisterStringForGlobalID(gStr_treeSeqRememberIndividuals, gID_treeSeqRememberIndividuals);
		Eidos_RegisterStringForGlobalID(gStr_treeSeqOutput, gID_treeSeqOutput);
		Eidos_RegisterStringForGlobalID(gStr_setMigrationRates, gID_setMigrationRates);
		Eidos_RegisterStringForGlobalID(gStr_pointInBounds, gID_pointInBounds);
		Eidos_RegisterStringForGlobalID(gStr_pointReflected, gID_pointReflected);
		Eidos_RegisterStringForGlobalID(gStr_pointStopped, gID_pointStopped);
		Eidos_RegisterStringForGlobalID(gStr_pointPeriodic, gID_pointPeriodic);
		Eidos_RegisterStringForGlobalID(gStr_pointUniform, gID_pointUniform);
		Eidos_RegisterStringForGlobalID(gStr_setCloningRate, gID_setCloningRate);
		Eidos_RegisterStringForGlobalID(gStr_setSelfingRate, gID_setSelfingRate);
		Eidos_RegisterStringForGlobalID(gStr_setSexRatio, gID_setSexRatio);
		Eidos_RegisterStringForGlobalID(gStr_setSpatialBounds, gID_setSpatialBounds);
		Eidos_RegisterStringForGlobalID(gStr_setSubpopulationSize, gID_setSubpopulationSize);
		Eidos_RegisterStringForGlobalID(gStr_addCloned, gID_addCloned);
		Eidos_RegisterStringForGlobalID(gStr_addCrossed, gID_addCrossed);
		Eidos_RegisterStringForGlobalID(gStr_addEmpty, gID_addEmpty);
		Eidos_RegisterStringForGlobalID(gStr_addRecombinant, gID_addRecombinant);
		Eidos_RegisterStringForGlobalID(gStr_addSelfed, gID_addSelfed);
		Eidos_RegisterStringForGlobalID(gStr_takeMigrants, gID_takeMigrants);
		Eidos_RegisterStringForGlobalID(gStr_removeSubpopulation, gID_removeSubpopulation);
		Eidos_RegisterStringForGlobalID(gStr_cachedFitness, gID_cachedFitness);
		Eidos_RegisterStringForGlobalID(gStr_sampleIndividuals, gID_sampleIndividuals);
		Eidos_RegisterStringForGlobalID(gStr_subsetIndividuals, gID_subsetIndividuals);
		Eidos_RegisterStringForGlobalID(gStr_defineSpatialMap, gID_defineSpatialMap);
		Eidos_RegisterStringForGlobalID(gStr_spatialMapColor, gID_spatialMapColor);
		Eidos_RegisterStringForGlobalID(gStr_spatialMapValue, gID_spatialMapValue);
		Eidos_RegisterStringForGlobalID(gStr_outputMSSample, gID_outputMSSample);
		Eidos_RegisterStringForGlobalID(gStr_outputVCFSample, gID_outputVCFSample);
		Eidos_RegisterStringForGlobalID(gStr_outputSample, gID_outputSample);
		Eidos_RegisterStringForGlobalID(gStr_outputMS, gID_outputMS);
		Eidos_RegisterStringForGlobalID(gStr_outputVCF, gID_outputVCF);
		Eidos_RegisterStringForGlobalID(gStr_output, gID_output);
		Eidos_RegisterStringForGlobalID(gStr_evaluate, gID_evaluate);
		Eidos_RegisterStringForGlobalID(gStr_distance, gID_distance);
		Eidos_RegisterStringForGlobalID(gStr_interactionDistance, gID_interactionDistance);
		Eidos_RegisterStringForGlobalID(gStr_distanceToPoint, gID_distanceToPoint);
		Eidos_RegisterStringForGlobalID(gStr_nearestNeighbors, gID_nearestNeighbors);
		Eidos_RegisterStringForGlobalID(gStr_nearestInteractingNeighbors, gID_nearestInteractingNeighbors);
		Eidos_RegisterStringForGlobalID(gStr_interactingNeighborCount, gID_interactingNeighborCount);
		Eidos_RegisterStringForGlobalID(gStr_nearestNeighborsOfPoint, gID_nearestNeighborsOfPoint);
		Eidos_RegisterStringForGlobalID(gStr_setInteractionFunction, gID_setInteractionFunction);
		Eidos_RegisterStringForGlobalID(gStr_strength, gID_strength);
		Eidos_RegisterStringForGlobalID(gStr_totalOfNeighborStrengths, gID_totalOfNeighborStrengths);
		Eidos_RegisterStringForGlobalID(gStr_unevaluate, gID_unevaluate);
		Eidos_RegisterStringForGlobalID(gStr_drawByStrength, gID_drawByStrength);
		
		Eidos_RegisterStringForGlobalID(gStr_sim, gID_sim);
		Eidos_RegisterStringForGlobalID(gStr_self, gID_self);
		Eidos_RegisterStringForGlobalID(gStr_individual, gID_individual);
		Eidos_RegisterStringForGlobalID(gStr_element, gID_element);
		Eidos_RegisterStringForGlobalID(gStr_genome, gID_genome);
		Eidos_RegisterStringForGlobalID(gStr_genome1, gID_genome1);
		Eidos_RegisterStringForGlobalID(gStr_genome2, gID_genome2);
		Eidos_RegisterStringForGlobalID(gStr_subpop, gID_subpop);
		Eidos_RegisterStringForGlobalID(gStr_sourceSubpop, gID_sourceSubpop);
		Eidos_RegisterStringForGlobalID(gStr_child, gID_child);
		Eidos_RegisterStringForGlobalID(gStr_childGenome1, gID_childGenome1);
		Eidos_RegisterStringForGlobalID(gStr_childGenome2, gID_childGenome2);
		Eidos_RegisterStringForGlobalID(gStr_childIsFemale, gID_childIsFemale);
		Eidos_RegisterStringForGlobalID(gStr_parent, gID_parent);
		Eidos_RegisterStringForGlobalID(gStr_parent1, gID_parent1);
		Eidos_RegisterStringForGlobalID(gStr_parent1Genome1, gID_parent1Genome1);
		Eidos_RegisterStringForGlobalID(gStr_parent1Genome2, gID_parent1Genome2);
		Eidos_RegisterStringForGlobalID(gStr_isCloning, gID_isCloning);
		Eidos_RegisterStringForGlobalID(gStr_isSelfing, gID_isSelfing);
		Eidos_RegisterStringForGlobalID(gStr_parent2, gID_parent2);
		Eidos_RegisterStringForGlobalID(gStr_parent2Genome1, gID_parent2Genome1);
		Eidos_RegisterStringForGlobalID(gStr_parent2Genome2, gID_parent2Genome2);
		Eidos_RegisterStringForGlobalID(gStr_mut, gID_mut);
		Eidos_RegisterStringForGlobalID(gStr_relFitness, gID_relFitness);
		Eidos_RegisterStringForGlobalID(gStr_homozygous, gID_homozygous);
		Eidos_RegisterStringForGlobalID(gStr_breakpoints, gID_breakpoints);
		Eidos_RegisterStringForGlobalID(gStr_receiver, gID_receiver);
		Eidos_RegisterStringForGlobalID(gStr_exerter, gID_exerter);
		Eidos_RegisterStringForGlobalID(gStr_originalNuc, gID_originalNuc);
		
		Eidos_RegisterStringForGlobalID(gStr_slimgui, gID_slimgui);
		Eidos_RegisterStringForGlobalID(gStr_pid, gID_pid);
		Eidos_RegisterStringForGlobalID(gStr_openDocument, gID_openDocument);
		Eidos_RegisterStringForGlobalID(gStr_pauseExecution, gID_pauseExecution);
		Eidos_RegisterStringForGlobalID(gStr_configureDisplay, gID_configureDisplay);
		
		Eidos_RegisterStringForGlobalID(gStr_Chromosome, gID_Chromosome);
		//Eidos_RegisterStringForGlobalID(gStr_Genome, gID_Genome);					// in Eidos; see EidosValue_Object::EidosValue_Object()
		Eidos_RegisterStringForGlobalID(gStr_GenomicElement, gID_GenomicElement);
		Eidos_RegisterStringForGlobalID(gStr_GenomicElementType, gID_GenomicElementType);
		//Eidos_RegisterStringForGlobalID(gStr_Mutation, gID_Mutation);				// in Eidos; see EidosValue_Object::EidosValue_Object()
		Eidos_RegisterStringForGlobalID(gStr_MutationType, gID_MutationType);
		Eidos_RegisterStringForGlobalID(gStr_SLiMEidosBlock, gID_SLiMEidosBlock);
		Eidos_RegisterStringForGlobalID(gStr_SLiMSim, gID_SLiMSim);
		Eidos_RegisterStringForGlobalID(gStr_Subpopulation, gID_Subpopulation);
		//Eidos_RegisterStringForGlobalID(gStr_Individual, gID_Individual);			// in Eidos; see EidosValue_Object::EidosValue_Object()
		Eidos_RegisterStringForGlobalID(gStr_Substitution, gID_Substitution);
		Eidos_RegisterStringForGlobalID(gStr_InteractionType, gID_InteractionType);
		Eidos_RegisterStringForGlobalID(gStr_SLiMgui, gID_SLiMgui);
		
		Eidos_RegisterStringForGlobalID(gStr_A, gID_A);
		Eidos_RegisterStringForGlobalID(gStr_X, gID_X);
		Eidos_RegisterStringForGlobalID(gStr_Y, gID_Y);
		Eidos_RegisterStringForGlobalID(gStr_f, gID_f);
		Eidos_RegisterStringForGlobalID(gStr_g, gID_g);
		Eidos_RegisterStringForGlobalID(gStr_e, gID_e);
		Eidos_RegisterStringForGlobalID(gStr_w, gID_w);
		Eidos_RegisterStringForGlobalID(gStr_l, gID_l);
		//Eidos_RegisterStringForGlobalID(gStr_s, gID_s);
		Eidos_RegisterStringForGlobalID(gStr_early, gID_early);
		Eidos_RegisterStringForGlobalID(gStr_late, gID_late);
		Eidos_RegisterStringForGlobalID(gStr_initialize, gID_initialize);
		Eidos_RegisterStringForGlobalID(gStr_fitness, gID_fitness);
		Eidos_RegisterStringForGlobalID(gStr_interaction, gID_interaction);
		Eidos_RegisterStringForGlobalID(gStr_mateChoice, gID_mateChoice);
		Eidos_RegisterStringForGlobalID(gStr_modifyChild, gID_modifyChild);
		Eidos_RegisterStringForGlobalID(gStr_recombination, gID_recombination);
		Eidos_RegisterStringForGlobalID(gStr_mutation, gID_mutation);
		Eidos_RegisterStringForGlobalID(gStr_reproduction, gID_reproduction);
	}
}

























































