//
//  slim_globals.cpp
//  SLiM
//
//  Created by Ben Haller on 1/4/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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
		float row0dists[] = {0, 3, 2};
		float row0strengths[] = {0.05f, 0.35f, 0.25f};
		uint32_t row1cols[] = {4};
		float row1dists[] = {4};
		float row1strengths[] = {1.45f};
		uint32_t row3cols[] = {4, 1};
		float row3dists[] = {4, 1};
		float row3strengths[] = {3.45f, 3.15f};
		
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
		
		sa.AddEntryInteraction(0, 0, 0, 0.05f);
		sa.AddEntryInteraction(0, 3, 3, 0.35f);
		sa.AddEntryInteraction(0, 2, 2, 0.25f);
		sa.AddEntryInteraction(1, 4, 4, 1.45f);
		sa.AddEntryInteraction(3, 4, 4, 3.45f);
		sa.AddEntryInteraction(3, 1, 1, 3.15f);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif

#if 0
	{
		// This should fail because row 1 is added twice
		SparseArray sa(5, 5);
		uint32_t row0cols[] = {0, 3, 2};
		float row0dists[] = {0, 3, 2};
		float row0strengths[] = {0.05f, 0.35f, 0.25f};
		uint32_t row1cols[] = {4};
		float row1dists[] = {4};
		float row1strengths[] = {1.45f};
		
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
		float row0dists[] = {0, 3, 2};
		float row0strengths[] = {0.05f, 0.35f, 0.25f};
		uint32_t row1cols[] = {4};
		float row1dists[] = {4};
		float row1strengths[] = {1.45f};
		
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
		float row1dists[] = {4};
		float row1strengths[] = {1.45f};
		
		sa.AddRowInteractions(1, row1cols, row1dists, row1strengths, 1);
	}
#endif
	
#if 0
	{
		// This should fail because rows are added out of order
		SparseArray sa(5, 5);
		
		sa.AddEntryInteraction(0, 0, 0, 0.05f);
		sa.AddEntryInteraction(1, 4, 4, 1.45f);
		sa.AddEntryInteraction(0, 3, 3, 0.35f);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif
	
#if 0
	{
		// This should fail because a row is added that is beyond bounds
		SparseArray sa(5, 5);
		
		sa.AddEntryInteraction(5, 0, 0, 0.05f);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif
	
#if 0
	{
		// This should fail because a column is added that is beyond bounds
		SparseArray sa(5, 5);
		
		sa.AddEntryInteraction(0, 5, 0, 0.05f);
		sa.Finished();
		
		std::cout << sa << std::endl;
	}
#endif
	
#if 0
	{
		// stress test by creating a large number of sparse arrays by entry and cross-checking them
		for (int trial = 0; trial < 10000; ++trial)
		{
			float *distances = (float *)calloc(100 * 100, sizeof(float));
			float *strengths = (float *)calloc(100 * 100, sizeof(float));
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
			float *distances = (float *)calloc(100 * 100, sizeof(double));
			float *strengths = (float *)calloc(100 * 100, sizeof(double));
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
				std::vector<float> row_distances;
				std::vector<float> row_strengths;
				
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
        MutationType *found_muttype = p_sim.MutationTypeWithID(mutation_type_id);
		
		if (!found_muttype)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractMutationTypeFromEidosValue_io): " << p_method_name << " mutation type m" << mutation_type_id << " not defined." << EidosTerminate();
		
		return found_muttype;
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
        GenomicElementType *found_getype = p_sim.GenomicElementTypeTypeWithID(getype_id);
		
		if (!found_getype)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractGenomicElementTypeFromEidosValue_io): " << p_method_name << " genomic element type g" << getype_id << " not defined." << EidosTerminate();
		
		return found_getype;
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
        Subpopulation *found_subpop = p_sim.SubpopulationWithID(source_subpop_id);
		
		if (!found_subpop)
			EIDOS_TERMINATION << "ERROR (SLiM_ExtractSubpopulationFromEidosValue_io): " << p_method_name << " subpopulation p" << source_subpop_id << " not defined." << EidosTerminate();
		
		return found_subpop;
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

// Verbosity, from the command-line option -l[ong]; defaults to 1 if -l[ong] is not used
long SLiM_verbosity_level = 1;


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
		nuc_lookup = (uint8_t *)malloc(256 * sizeof(uint8_t));
		
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
	
	// Eat 32 nucleotides at a time if we can
	std::size_t index = 0, buf_index = 0;
	
	for ( ; index < length_; index += 32)
	{
		uint64_t accumulator = 0;
		
		for (std::size_t i = 0; i < 32; )
		{
			char nuc_char = p_char_buffer[index + i];
			uint64_t nuc = nuc_lookup[(int)(nuc_char)];
			
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
const std::string &gStr_originGeneration = EidosRegisteredString("originGeneration", gID_originGeneration);
const std::string &gStr_position = EidosRegisteredString("position", gID_position);
const std::string &gStr_selectionCoeff = EidosRegisteredString("selectionCoeff", gID_selectionCoeff);
const std::string &gStr_subpopID = EidosRegisteredString("subpopID", gID_subpopID);
const std::string &gStr_convertToSubstitution = EidosRegisteredString("convertToSubstitution", gID_convertToSubstitution);
const std::string &gStr_distributionType = EidosRegisteredString("distributionType", gID_distributionType);
const std::string &gStr_distributionParams = EidosRegisteredString("distributionParams", gID_distributionParams);
const std::string &gStr_dominanceCoeff = EidosRegisteredString("dominanceCoeff", gID_dominanceCoeff);
const std::string &gStr_mutationStackGroup = EidosRegisteredString("mutationStackGroup", gID_mutationStackGroup);
const std::string &gStr_mutationStackPolicy = EidosRegisteredString("mutationStackPolicy", gID_mutationStackPolicy);
//const std::string &gStr_start = EidosRegisteredString("start", gID_start);
//const std::string &gStr_end = EidosRegisteredString("end", gID_end);
//const std::string &gStr_type = EidosRegisteredString("type", gID_type);
//const std::string &gStr_source = EidosRegisteredString("source", gID_source);
const std::string &gStr_active = EidosRegisteredString("active", gID_active);
const std::string &gStr_chromosome = EidosRegisteredString("chromosome", gID_chromosome);
const std::string &gStr_chromosomeType = EidosRegisteredString("chromosomeType", gID_chromosomeType);
const std::string &gStr_genomicElementTypes = EidosRegisteredString("genomicElementTypes", gID_genomicElementTypes);
const std::string &gStr_inSLiMgui = EidosRegisteredString("inSLiMgui", gID_inSLiMgui);
const std::string &gStr_interactionTypes = EidosRegisteredString("interactionTypes", gID_interactionTypes);
const std::string &gStr_modelType = EidosRegisteredString("modelType", gID_modelType);
const std::string &gStr_nucleotideBased = EidosRegisteredString("nucleotideBased", gID_nucleotideBased);
const std::string &gStr_scriptBlocks = EidosRegisteredString("scriptBlocks", gID_scriptBlocks);
const std::string &gStr_sexEnabled = EidosRegisteredString("sexEnabled", gID_sexEnabled);
const std::string &gStr_subpopulations = EidosRegisteredString("subpopulations", gID_subpopulations);
const std::string &gStr_substitutions = EidosRegisteredString("substitutions", gID_substitutions);
const std::string &gStr_dominanceCoeffX = EidosRegisteredString("dominanceCoeffX", gID_dominanceCoeffX);
const std::string &gStr_generation = EidosRegisteredString("generation", gID_generation);
const std::string &gStr_colorSubstitution = EidosRegisteredString("colorSubstitution", gID_colorSubstitution);
const std::string &gStr_tag = EidosRegisteredString("tag", gID_tag);
const std::string &gStr_tagF = EidosRegisteredString("tagF", gID_tagF);
const std::string &gStr_migrant = EidosRegisteredString("migrant", gID_migrant);
const std::string &gStr_fitnessScaling = EidosRegisteredString("fitnessScaling", gID_fitnessScaling);
const std::string &gStr_firstMaleIndex = EidosRegisteredString("firstMaleIndex", gID_firstMaleIndex);
const std::string &gStr_genomes = EidosRegisteredString("genomes", gID_genomes);
const std::string &gStr_sex = EidosRegisteredString("sex", gID_sex);
const std::string &gStr_individuals = EidosRegisteredString("individuals", gID_individuals);
const std::string &gStr_subpopulation = EidosRegisteredString("subpopulation", gID_subpopulation);
const std::string &gStr_index = EidosRegisteredString("index", gID_index);
const std::string &gStr_immigrantSubpopIDs = EidosRegisteredString("immigrantSubpopIDs", gID_immigrantSubpopIDs);
const std::string &gStr_immigrantSubpopFractions = EidosRegisteredString("immigrantSubpopFractions", gID_immigrantSubpopFractions);
const std::string &gStr_selfingRate = EidosRegisteredString("selfingRate", gID_selfingRate);
const std::string &gStr_cloningRate = EidosRegisteredString("cloningRate", gID_cloningRate);
const std::string &gStr_sexRatio = EidosRegisteredString("sexRatio", gID_sexRatio);
const std::string &gStr_spatialBounds = EidosRegisteredString("spatialBounds", gID_spatialBounds);
const std::string &gStr_individualCount = EidosRegisteredString("individualCount", gID_individualCount);
const std::string &gStr_fixationGeneration = EidosRegisteredString("fixationGeneration", gID_fixationGeneration);
const std::string &gStr_age = EidosRegisteredString("age", gID_age);
const std::string &gStr_pedigreeID = EidosRegisteredString("pedigreeID", gID_pedigreeID);
const std::string &gStr_pedigreeParentIDs = EidosRegisteredString("pedigreeParentIDs", gID_pedigreeParentIDs);
const std::string &gStr_pedigreeGrandparentIDs = EidosRegisteredString("pedigreeGrandparentIDs", gID_pedigreeGrandparentIDs);
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
const std::string &gStr_mutationFrequencies = EidosRegisteredString("mutationFrequencies", gID_mutationFrequencies);
const std::string &gStr_mutationCounts = EidosRegisteredString("mutationCounts", gID_mutationCounts);
//const std::string &gStr_mutationsOfType = EidosRegisteredString("mutationsOfType", gID_mutationsOfType);
//const std::string &gStr_countOfMutationsOfType = EidosRegisteredString("countOfMutationsOfType", gID_countOfMutationsOfType);
const std::string &gStr_outputFixedMutations = EidosRegisteredString("outputFixedMutations", gID_outputFixedMutations);
const std::string &gStr_outputFull = EidosRegisteredString("outputFull", gID_outputFull);
const std::string &gStr_outputMutations = EidosRegisteredString("outputMutations", gID_outputMutations);
const std::string &gStr_outputUsage = EidosRegisteredString("outputUsage", gID_outputUsage);
const std::string &gStr_readFromPopulationFile = EidosRegisteredString("readFromPopulationFile", gID_readFromPopulationFile);
const std::string &gStr_recalculateFitness = EidosRegisteredString("recalculateFitness", gID_recalculateFitness);
const std::string &gStr_registerEarlyEvent = EidosRegisteredString("registerEarlyEvent", gID_registerEarlyEvent);
const std::string &gStr_registerLateEvent = EidosRegisteredString("registerLateEvent", gID_registerLateEvent);
const std::string &gStr_registerFitnessCallback = EidosRegisteredString("registerFitnessCallback", gID_registerFitnessCallback);
const std::string &gStr_registerInteractionCallback = EidosRegisteredString("registerInteractionCallback", gID_registerInteractionCallback);
const std::string &gStr_registerMateChoiceCallback = EidosRegisteredString("registerMateChoiceCallback", gID_registerMateChoiceCallback);
const std::string &gStr_registerModifyChildCallback = EidosRegisteredString("registerModifyChildCallback", gID_registerModifyChildCallback);
const std::string &gStr_registerRecombinationCallback = EidosRegisteredString("registerRecombinationCallback", gID_registerRecombinationCallback);
const std::string &gStr_registerMutationCallback = EidosRegisteredString("registerMutationCallback", gID_registerMutationCallback);
const std::string &gStr_registerReproductionCallback = EidosRegisteredString("registerReproductionCallback", gID_registerReproductionCallback);
const std::string &gStr_rescheduleScriptBlock = EidosRegisteredString("rescheduleScriptBlock", gID_rescheduleScriptBlock);
const std::string &gStr_simulationFinished = EidosRegisteredString("simulationFinished", gID_simulationFinished);
const std::string &gStr_subsetMutations = EidosRegisteredString("subsetMutations", gID_subsetMutations);
const std::string &gStr_treeSeqCoalesced = EidosRegisteredString("treeSeqCoalesced", gID_treeSeqCoalesced);
const std::string &gStr_treeSeqSimplify = EidosRegisteredString("treeSeqSimplify", gID_treeSeqSimplify);
const std::string &gStr_treeSeqRememberIndividuals = EidosRegisteredString("treeSeqRememberIndividuals", gID_treeSeqRememberIndividuals);
const std::string &gStr_treeSeqOutput = EidosRegisteredString("treeSeqOutput", gID_treeSeqOutput);
const std::string &gStr_setMigrationRates = EidosRegisteredString("setMigrationRates", gID_setMigrationRates);
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
const std::string &gStr_spatialMapColor = EidosRegisteredString("spatialMapColor", gID_spatialMapColor);
const std::string &gStr_spatialMapValue = EidosRegisteredString("spatialMapValue", gID_spatialMapValue);
const std::string &gStr_outputMSSample = EidosRegisteredString("outputMSSample", gID_outputMSSample);
const std::string &gStr_outputVCFSample = EidosRegisteredString("outputVCFSample", gID_outputVCFSample);
const std::string &gStr_outputSample = EidosRegisteredString("outputSample", gID_outputSample);
const std::string &gStr_outputMS = EidosRegisteredString("outputMS", gID_outputMS);
const std::string &gStr_outputVCF = EidosRegisteredString("outputVCF", gID_outputVCF);
const std::string &gStr_output = EidosRegisteredString("output", gID_output);
const std::string &gStr_evaluate = EidosRegisteredString("evaluate", gID_evaluate);
const std::string &gStr_distance = EidosRegisteredString("distance", gID_distance);
const std::string &gStr_interactionDistance = EidosRegisteredString("interactionDistance", gID_interactionDistance);
const std::string &gStr_distanceToPoint = EidosRegisteredString("distanceToPoint", gID_distanceToPoint);
const std::string &gStr_nearestNeighbors = EidosRegisteredString("nearestNeighbors", gID_nearestNeighbors);
const std::string &gStr_nearestInteractingNeighbors = EidosRegisteredString("nearestInteractingNeighbors", gID_nearestInteractingNeighbors);
const std::string &gStr_interactingNeighborCount = EidosRegisteredString("interactingNeighborCount", gID_interactingNeighborCount);
const std::string &gStr_nearestNeighborsOfPoint = EidosRegisteredString("nearestNeighborsOfPoint", gID_nearestNeighborsOfPoint);
const std::string &gStr_setInteractionFunction = EidosRegisteredString("setInteractionFunction", gID_setInteractionFunction);
const std::string &gStr_strength = EidosRegisteredString("strength", gID_strength);
const std::string &gStr_totalOfNeighborStrengths = EidosRegisteredString("totalOfNeighborStrengths", gID_totalOfNeighborStrengths);
const std::string &gStr_unevaluate = EidosRegisteredString("unevaluate", gID_unevaluate);
const std::string &gStr_drawByStrength = EidosRegisteredString("drawByStrength", gID_drawByStrength);

// mostly SLiM variable names used in callbacks and such
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
const std::string &gStr_childGenome1 = EidosRegisteredString("childGenome1", gID_childGenome1);
const std::string &gStr_childGenome2 = EidosRegisteredString("childGenome2", gID_childGenome2);
const std::string &gStr_childIsFemale = EidosRegisteredString("childIsFemale", gID_childIsFemale);
const std::string &gStr_parent = EidosRegisteredString("parent", gID_parent);
const std::string &gStr_parent1 = EidosRegisteredString("parent1", gID_parent1);
const std::string &gStr_parent1Genome1 = EidosRegisteredString("parent1Genome1", gID_parent1Genome1);
const std::string &gStr_parent1Genome2 = EidosRegisteredString("parent1Genome2", gID_parent1Genome2);
const std::string &gStr_isCloning = EidosRegisteredString("isCloning", gID_isCloning);
const std::string &gStr_isSelfing = EidosRegisteredString("isSelfing", gID_isSelfing);
const std::string &gStr_parent2 = EidosRegisteredString("parent2", gID_parent2);
const std::string &gStr_parent2Genome1 = EidosRegisteredString("parent2Genome1", gID_parent2Genome1);
const std::string &gStr_parent2Genome2 = EidosRegisteredString("parent2Genome2", gID_parent2Genome2);
const std::string &gStr_mut = EidosRegisteredString("mut", gID_mut);
const std::string &gStr_relFitness = EidosRegisteredString("relFitness", gID_relFitness);
const std::string &gStr_homozygous = EidosRegisteredString("homozygous", gID_homozygous);
const std::string &gStr_breakpoints = EidosRegisteredString("breakpoints", gID_breakpoints);
const std::string &gStr_receiver = EidosRegisteredString("receiver", gID_receiver);
const std::string &gStr_exerter = EidosRegisteredString("exerter", gID_exerter);
const std::string &gStr_originalNuc = EidosRegisteredString("originalNuc", gID_originalNuc);

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
const std::string &gStr_SLiMSim = EidosRegisteredString("SLiMSim", gID_SLiMSim);
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
const std::string &gStr_addCustomColumn = EidosRegisteredString("addCustomColumn", gID_addCustomColumn);
const std::string &gStr_addGeneration = EidosRegisteredString("addGeneration", gID_addGeneration);
const std::string &gStr_addGenerationStage = EidosRegisteredString("addGenerationStage", gID_addGenerationStage);
const std::string &gStr_addMeanSDColumns = EidosRegisteredString("addMeanSDColumns", gID_addMeanSDColumns);
const std::string &gStr_addPopulationSexRatio = EidosRegisteredString("addPopulationSexRatio", gID_addPopulationSexRatio);
const std::string &gStr_addPopulationSize = EidosRegisteredString("addPopulationSize", gID_addPopulationSize);
const std::string &gStr_addSubpopulationSexRatio = EidosRegisteredString("addSubpopulationSexRatio", gID_addSubpopulationSexRatio);
const std::string &gStr_addSubpopulationSize = EidosRegisteredString("addSubpopulationSize", gID_addSubpopulationSize);
const std::string &gStr_flush = EidosRegisteredString("flush", gID_flush);
const std::string &gStr_logRow = EidosRegisteredString("logRow", gID_logRow);
const std::string &gStr_setLogInterval = EidosRegisteredString("setLogInterval", gID_setLogInterval);
const std::string &gStr_setFilePath = EidosRegisteredString("setFilePath", gID_setFilePath);
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
//const std::string &gStr_s = EidosRegisteredString("s", gID_s);		now gEidosStr_s
const std::string &gStr_early = EidosRegisteredString("early", gID_early);
const std::string &gStr_late = EidosRegisteredString("late", gID_late);
const std::string &gStr_initialize = EidosRegisteredString("initialize", gID_initialize);
const std::string &gStr_fitness = EidosRegisteredString("fitness", gID_fitness);
const std::string &gStr_interaction = EidosRegisteredString("interaction", gID_interaction);
const std::string &gStr_mateChoice = EidosRegisteredString("mateChoice", gID_mateChoice);
const std::string &gStr_modifyChild = EidosRegisteredString("modifyChild", gID_modifyChild);
const std::string &gStr_recombination = EidosRegisteredString("recombination", gID_recombination);
const std::string &gStr_mutation = EidosRegisteredString("mutation", gID_mutation);
const std::string &gStr_reproduction = EidosRegisteredString("reproduction", gID_reproduction);


void SLiM_ConfigureContext(void)
{
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		gEidosContextVersion = SLIM_VERSION_FLOAT;
		gEidosContextVersionString = std::string("SLiM version ") + std::string(SLIM_VERSION_STRING);
		gEidosContextLicense = "SLiM is free software: you can redistribute it and/or\nmodify it under the terms of the GNU General Public\nLicense as published by the Free Software Foundation,\neither version 3 of the License, or (at your option)\nany later version.\n\nSLiM is distributed in the hope that it will be\nuseful, but WITHOUT ANY WARRANTY; without even the\nimplied warranty of MERCHANTABILITY or FITNESS FOR\nA PARTICULAR PURPOSE.  See the GNU General Public\nLicense for more details.\n\nYou should have received a copy of the GNU General\nPublic License along with SLiM.  If not, see\n<http://www.gnu.org/licenses/>.\n";
		gEidosContextCitation = "To cite SLiM in publications please use:\n\nHaller, B.C., and Messer, P.W. (2019). SLiM 3: Forward\ngenetic simulations beyond the Wright-Fisher model.\nMolecular Biology and Evolution 36(3), 632-637.\nDOI: https://doi.org/10.1093/molbev/msy228\n\nFor papers using tree-sequence recording, please cite:\n\nHaller, B.C., Galloway, J., Kelleher, J., Messer, P.W.,\n& Ralph, P.L. (2019). Tree‐sequence recording in SLiM\nopens new horizons for forward‐time simulation of whole\ngenomes. Molecular Ecology Resources 19(2), 552-566.\nDOI: https://doi.org/10.1111/1755-0998.12968\n";
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
 

const std::string gSLiM_tsk_metadata_schema =
"{\"$schema\":\"http://json-schema.org/schema#\",\"codec\":\"json\",\"examples\":[{\"SLiM\":{\"file_version\":\"0.5\",\"generation\":123,\"model_type\":\"WF\",\"nucleotide_based\":false,\"separate_sexes\":true,\"spatial_dimensionality\":\"xy\",\"spatial_periodicity\":\"x\"}}],\"properties\":{\"SLiM\":{\"description\":\"Top-level metadata for a SLiM tree sequence, file format version 0.5\",\"properties\":{\"file_version\":{\"description\":\"The SLiM 'file format version' of this tree sequence.\",\"type\":\"string\"},\"generation\":{\"description\":\"The 'SLiM generation' counter when this tree sequence was recorded.\",\"type\":\"integer\"},\"model_type\":{\"description\":\"The model type used for the last part of this simulation (WF or nonWF).\",\"enum\":[\"WF\",\"nonWF\"],\"type\":\"string\"},\"nucleotide_based\":{\"description\":\"Whether the simulation was nucleotide-based.\",\"type\":\"boolean\"},\"separate_sexes\":{\"description\":\"Whether the simulation had separate sexes.\",\"type\":\"boolean\"},\"spatial_dimensionality\":{\"description\":\"The spatial dimensionality of the simulation.\",\"enum\":[\"\",\"x\",\"xy\",\"xyz\"],\"type\":\"string\"},\"spatial_periodicity\":{\"description\":\"The spatial periodicity of the simulation.\",\"enum\":[\"\",\"x\",\"y\",\"z\",\"xy\",\"xz\",\"yz\",\"xyz\"],\"type\":\"string\"},\"stage\":{\"description\":\"The stage of the SLiM life cycle when this tree sequence was recorded.\",\"type\":\"string\"}},\"required\":[\"model_type\",\"generation\",\"file_version\",\"spatial_dimensionality\",\"spatial_periodicity\",\"separate_sexes\",\"nucleotide_based\"],\"type\":\"object\"}},\"required\":[\"SLiM\"],\"type\":\"object\"}";

const std::string gSLiM_tsk_edge_metadata_schema = "";
const std::string gSLiM_tsk_site_metadata_schema = "";

const std::string gSLiM_tsk_mutation_metadata_schema =
"{\"$schema\":\"http://json-schema.org/schema#\",\"additionalProperties\":false,\"codec\":\"struct\",\"description\":\"SLiM schema for mutation metadata.\",\"examples\":[{\"mutation_list\":[{\"mutation_type\":1,\"nucleotide\":3,\"selection_coeff\":-0.2,\"slim_time\":243,\"subpopulation\":0}]}],\"properties\":{\"mutation_list\":{\"items\":{\"additionalProperties\":false,\"properties\":{\"mutation_type\":{\"binaryFormat\":\"i\",\"description\":\"The index of this mutation's mutationType.\",\"index\":1,\"type\":\"integer\"},\"nucleotide\":{\"binaryFormat\":\"b\",\"description\":\"The nucleotide for this mutation (0=A , 1=C , 2=G, 3=T, or -1 for none)\",\"index\":5,\"type\":\"integer\"},\"selection_coeff\":{\"binaryFormat\":\"f\",\"description\":\"This mutation's selection coefficient.\",\"index\":2,\"type\":\"number\"},\"slim_time\":{\"binaryFormat\":\"i\",\"description\":\"The SLiM generation counter when this mutation occurred.\",\"index\":4,\"type\":\"integer\"},\"subpopulation\":{\"binaryFormat\":\"i\",\"description\":\"The ID of the subpopulation this mutation occurred in.\",\"index\":3,\"type\":\"integer\"}},\"required\":[\"mutation_type\",\"selection_coeff\",\"subpopulation\",\"slim_time\",\"nucleotide\"],\"type\":\"object\"},\"noLengthEncodingExhaustBuffer\":true,\"type\":\"array\"}},\"required\":[\"mutation_list\"],\"type\":\"object\"}";

const std::string gSLiM_tsk_node_metadata_schema =
"{\"$schema\":\"http://json-schema.org/schema#\",\"additionalProperties\":false,\"codec\":\"struct\",\"description\":\"SLiM schema for node metadata.\",\"examples\":[{\"genome_type\":0,\"is_null\":false,\"slim_id\":123}],\"properties\":{\"genome_type\":{\"binaryFormat\":\"B\",\"description\":\"The 'type' of this genome (0 for autosome, 1 for X, 2 for Y).\",\"index\":2,\"type\":\"integer\"},\"is_null\":{\"binaryFormat\":\"?\",\"description\":\"Whether this node describes a 'null' (non-existant) chromosome.\",\"index\":1,\"type\":\"boolean\"},\"slim_id\":{\"binaryFormat\":\"q\",\"description\":\"The 'pedigree ID' of this chromosome in SLiM.\",\"index\":0,\"type\":\"integer\"}},\"required\":[\"slim_id\",\"is_null\",\"genome_type\"],\"type\":[\"object\",\"null\"]}";

const std::string gSLiM_tsk_individual_metadata_schema =
"{\"$schema\":\"http://json-schema.org/schema#\",\"additionalProperties\":false,\"codec\":\"struct\",\"description\":\"SLiM schema for individual metadata.\",\"examples\":[{\"age\":-1,\"flags\":0,\"pedigree_id\":123,\"sex\":0,\"subpopulation\":0}],\"flags\":{\"SLIM_INDIVIDUAL_METADATA_MIGRATED\":{\"description\":\"Whether this individual was a migrant, either in the generation when the tree sequence was written out (if the individual was alive then), or in the generation of the last time they were Remembered (if not).\",\"value\":1}},\"properties\":{\"age\":{\"binaryFormat\":\"i\",\"description\":\"The age of this individual, either when the tree sequence was written out (if the individual was alive then), or the last time they were Remembered (if not).\",\"index\":2,\"type\":\"integer\"},\"flags\":{\"binaryFormat\":\"I\",\"description\":\"Other information about the individual: see 'flags'.\",\"index\":5,\"type\":\"integer\"},\"pedigree_id\":{\"binaryFormat\":\"q\",\"description\":\"The 'pedigree ID' of this individual in SLiM.\",\"index\":1,\"type\":\"integer\"},\"sex\":{\"binaryFormat\":\"i\",\"description\":\"The sex of the individual (0 for female, 1 for male, -1 for hermaphrodite).\",\"index\":4,\"type\":\"integer\"},\"subpopulation\":{\"binaryFormat\":\"i\",\"description\":\"The ID of the subpopulation the individual was part of, either when the tree sequence was written out (if the individual was alive then), or the last time they were Remembered (if not).\",\"index\":3,\"type\":\"integer\"}},\"required\":[\"pedigree_id\",\"age\",\"subpopulation\",\"sex\",\"flags\"],\"type\":\"object\"}";

const std::string gSLiM_tsk_population_metadata_schema = 
"{\"$schema\":\"http://json-schema.org/schema#\",\"additionalProperties\":false,\"codec\":\"struct\",\"description\":\"SLiM schema for population metadata.\",\"examples\":[{\"bounds_x0\":0.0,\"bounds_x1\":100.0,\"bounds_y0\":0.0,\"bounds_y1\":100.0,\"bounds_z0\":0.0,\"bounds_z1\":100.0,\"female_cloning_fraction\":0.25,\"male_cloning_fraction\":0.0,\"migration_records\":[{\"migration_rate\":0.9,\"source_subpop\":1},{\"migration_rate\":0.1,\"source_subpop\":2}],\"selfing_fraction\":0.5,\"sex_ratio\":0.5,\"slim_id\":2}],\""
"properties\":{\"bounds_x0\":{\"binaryFormat\":\"d\",\"description\":\"The minimum x-coordinate in this subpopulation.\",\"index\":6,\"type\":\"number\"},\"bounds_x1\":{\"binaryFormat\":\"d\",\"description\":\"The maximum x-coordinate in this subpopulation.\",\"index\":7,\"type\":\"number\"},\"bounds_y0\":{\"binaryFormat\":\"d\",\"description\":\"The minimum y-coordinate in this subpopulation.\",\"index\":8,\"type\":\"number\"},\"bounds_y1\":{\"binaryFormat\":\"d\",\"description\":\"The maximum y-coordinate in this subpopulation.\",\"index\":9,\"type\":\"number\"},\"bounds_z0\":{\"binaryFormat\":\"d\",\"description\":\"The minimum z-coordinate in this subpopulation.\",\"index\":10,\"type\":\"number\"},\"bounds_z1\":{\"binaryFormat\":\"d\",\"description\":\"The maximum z-coordinate in this subpopulation.\",\"index\":11,\"type\":\"number\"},\"female_cloning_fraction\":{\"binaryFormat\":\"d\",\"description\":\"The frequency with which females in this subpopulation reproduce clonally (for WF models).\",\"index\":3,\"type\":\"number\"},\"male_cloning_fraction\":{\"binaryFormat\":\"d\",\"description\":\"The frequency with which males in this subpopulation reproduce clonally (for WF models).\",\"index\":4,\"type\":\"number\"},\"migration_records\":{\"arrayLengthFormat\":\"I\",\"index\":13,\"items\":{\"additionalProperties\":false,\""
"properties\":{\"migration_rate\":{\"binaryFormat\":\"d\",\"description\":\"The fraction of children in this subpopulation that are composed of 'migrants' from the source subpopulation (in WF models).\",\"index\":2,\"type\":\"number\"},\"source_subpop\":{\"binaryFormat\":\"i\",\"description\":\"The ID of the subpopulation migrants come from (in WF models).\",\"index\":1,\"type\":\"integer\"}},\"required\":[\"source_subpop\",\"migration_rate\"],\"type\":\"object\"},\"type\":\"array\"},\"selfing_fraction\":{\"binaryFormat\":\"d\",\"description\":\"The frequency with which individuals in this subpopulation self (for WF models).\",\"index\":2,\"type\":\"number\"},\"sex_ratio\":{\"binaryFormat\":\"d\",\"description\":\"This subpopulation's sex ratio (for WF models).\",\"index\":5,\"type\":\"number\"},\"slim_id\":{\"binaryFormat\":\"i\",\"description\":\"The ID of this population in SLiM. Note that this is called a 'subpopulation' in SLiM.\",\"index\":1,\"type\":\"integer\"}},\"required\":[\"slim_id\",\"selfing_fraction\",\"female_cloning_fraction\",\"male_cloning_fraction\",\"sex_ratio\",\"bounds_x0\",\"bounds_x1\",\"bounds_y0\",\"bounds_y1\",\"bounds_z0\",\"bounds_z1\",\"migration_records\"],\"type\":[\"object\",\"null\"]}";























































