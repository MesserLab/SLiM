//
//  slim_functions.cpp
//  SLiM
//
//  Created by Ben Haller on 2/15/19.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
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


#include "slim_functions.h"
#include "slim_globals.h"
#include "slim_sim.h"
#include "eidos_rng.h"

#include <string>
#include <vector>


const std::vector<EidosFunctionSignature_CSP> *SLiMSim::SLiMFunctionSignatures(void)
{
	// Allocate our own EidosFunctionSignature objects
	static std::vector<EidosFunctionSignature_CSP> sim_func_signatures_;
	
	if (!sim_func_signatures_.size())
	{
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("codonsToAminoAcids", SLiM_ExecuteFunction_codonsToAminoAcids, kEidosValueMaskString | kEidosValueMaskInt, "SLiM"))->AddInt("codons")->AddArgWithDefault(kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskOptional | kEidosValueMaskSingleton, "long", nullptr, gStaticEidosValue_LogicalF)->AddLogical_OS("paste", gStaticEidosValue_LogicalT));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mm16To256", SLiM_ExecuteFunction_mm16To256, kEidosValueMaskFloat, "SLiM"))->AddFloat("mutationMatrix16"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mmJukesCantor", SLiM_ExecuteFunction_mmJukesCantor, kEidosValueMaskFloat, "SLiM"))->AddFloat_S("alpha"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("mmKimura", SLiM_ExecuteFunction_mmKimura, kEidosValueMaskFloat, "SLiM"))->AddFloat_S("alpha")->AddFloat_S("beta"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideCounts", SLiM_ExecuteFunction_nucleotideCounts, kEidosValueMaskInt, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideFrequencies", SLiM_ExecuteFunction_nucleotideFrequencies, kEidosValueMaskFloat, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotidesToCodons", SLiM_ExecuteFunction_nucleotidesToCodons, kEidosValueMaskInt, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("randomNucleotides", SLiM_ExecuteFunction_randomNucleotides, kEidosValueMaskInt | kEidosValueMaskString, "SLiM"))->AddInt_S("length")->AddNumeric_ON("basis", gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string"))));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("codonsToNucleotides", SLiM_ExecuteFunction_codonsToNucleotides, kEidosValueMaskInt | kEidosValueMaskString, "SLiM"))->AddInt("codons")->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string"))));
	}
	
	return &sim_func_signatures_;
}

static std::string codon2aa_short[64] = {
	/* AAA */	"K",
	/* AAC */	"N",
	/* AAG */	"K",
	/* AAT */	"N",
	/* ACA */	"T",
	/* ACC */	"T",
	/* ACG */	"T",
	/* ACT */	"T",
	/* AGA */	"R",
	/* AGC */	"S",
	/* AGG */	"R",
	/* AGT */	"S",
	/* ATA */	"I",
	/* ATC */	"I",
	/* ATG */	"M",
	/* ATT */	"I",
	/* CAA */	"Q",
	/* CAC */	"H",
	/* CAG */	"Q",
	/* CAT */	"H",
	/* CCA */	"P",
	/* CCC */	"P",
	/* CCG */	"P",
	/* CCT */	"P",
	/* CGA */	"R",
	/* CGC */	"R",
	/* CGG */	"R",
	/* CGT */	"R",
	/* CTA */	"L",
	/* CTC */	"L",
	/* CTG */	"L",
	/* CTT */	"L",
	/* GAA */	"E",
	/* GAC */	"D",
	/* GAG */	"E",
	/* GAT */	"D",
	/* GCA */	"A",
	/* GCC */	"A",
	/* GCG */	"A",
	/* GCT */	"A",
	/* GGA */	"G",
	/* GGC */	"G",
	/* GGG */	"G",
	/* GGT */	"G",
	/* GTA */	"V",
	/* GTC */	"V",
	/* GTG */	"V",
	/* GTT */	"V",
	/* TAA */	"X",
	/* TAC */	"Y",
	/* TAG */	"X",
	/* TAT */	"Y",
	/* TCA */	"S",
	/* TCC */	"S",
	/* TCG */	"S",
	/* TCT */	"S",
	/* TGA */	"X",
	/* TGC */	"C",
	/* TGG */	"W",
	/* TGT */	"C",
	/* TTA */	"L",
	/* TTC */	"F",
	/* TTG */	"L",
	/* TTT */	"F"
};

static std::string codon2aa_long[64] = {
	/* AAA */	"Lys",
	/* AAC */	"Asn",
	/* AAG */	"Lys",
	/* AAT */	"Asn",
	/* ACA */	"Thr",
	/* ACC */	"Thr",
	/* ACG */	"Thr",
	/* ACT */	"Thr",
	/* AGA */	"Arg",
	/* AGC */	"Ser",
	/* AGG */	"Arg",
	/* AGT */	"Ser",
	/* ATA */	"Ile",
	/* ATC */	"Ile",
	/* ATG */	"Met",
	/* ATT */	"Ile",
	/* CAA */	"Gln",
	/* CAC */	"His",
	/* CAG */	"Gln",
	/* CAT */	"His",
	/* CCA */	"Pro",
	/* CCC */	"Pro",
	/* CCG */	"Pro",
	/* CCT */	"Pro",
	/* CGA */	"Arg",
	/* CGC */	"Arg",
	/* CGG */	"Arg",
	/* CGT */	"Arg",
	/* CTA */	"Leu",
	/* CTC */	"Leu",
	/* CTG */	"Leu",
	/* CTT */	"Leu",
	/* GAA */	"Glu",
	/* GAC */	"Asp",
	/* GAG */	"Glu",
	/* GAT */	"Asp",
	/* GCA */	"Ala",
	/* GCC */	"Ala",
	/* GCG */	"Ala",
	/* GCT */	"Ala",
	/* GGA */	"Gly",
	/* GGC */	"Gly",
	/* GGG */	"Gly",
	/* GGT */	"Gly",
	/* GTA */	"Val",
	/* GTC */	"Val",
	/* GTG */	"Val",
	/* GTT */	"Val",
	/* TAA */	"Ter",
	/* TAC */	"Tyr",
	/* TAG */	"Ter",
	/* TAT */	"Tyr",
	/* TCA */	"Ser",
	/* TCC */	"Ser",
	/* TCG */	"Ser",
	/* TCT */	"Ser",
	/* TGA */	"Ter",
	/* TGC */	"Cys",
	/* TGG */	"Trp",
	/* TGT */	"Cys",
	/* TTA */	"Leu",
	/* TTC */	"Phe",
	/* TTG */	"Leu",
	/* TTT */	"Phe"
};

static int codon2aa_int[64] = {
	/* AAA */	12,
	/* AAC */	3,
	/* AAG */	12,
	/* AAT */	3,
	/* ACA */	17,
	/* ACC */	17,
	/* ACG */	17,
	/* ACT */	17,
	/* AGA */	2,
	/* AGC */	16,
	/* AGG */	2,
	/* AGT */	16,
	/* ATA */	10,
	/* ATC */	10,
	/* ATG */	13,
	/* ATT */	10,
	/* CAA */	6,
	/* CAC */	9,
	/* CAG */	6,
	/* CAT */	9,
	/* CCA */	15,
	/* CCC */	15,
	/* CCG */	15,
	/* CCT */	15,
	/* CGA */	2,
	/* CGC */	2,
	/* CGG */	2,
	/* CGT */	2,
	/* CTA */	11,
	/* CTC */	11,
	/* CTG */	11,
	/* CTT */	11,
	/* GAA */	7,
	/* GAC */	4,
	/* GAG */	7,
	/* GAT */	4,
	/* GCA */	1,
	/* GCC */	1,
	/* GCG */	1,
	/* GCT */	1,
	/* GGA */	8,
	/* GGC */	8,
	/* GGG */	8,
	/* GGT */	8,
	/* GTA */	20,
	/* GTC */	20,
	/* GTG */	20,
	/* GTT */	20,
	/* TAA */	0,
	/* TAC */	19,
	/* TAG */	0,
	/* TAT */	19,
	/* TCA */	16,
	/* TCC */	16,
	/* TCG */	16,
	/* TCT */	16,
	/* TGA */	0,
	/* TGC */	5,
	/* TGG */	18,
	/* TGT */	5,
	/* TTA */	11,
	/* TTC */	14,
	/* TTG */	11,
	/* TTT */	14
};

//	(string)codonsToAminoAcids(integer codons, [li$ long = F])
EidosValue_SP SLiM_ExecuteFunction_codonsToAminoAcids(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *codons_value = p_arguments[0].get();
	EidosValue *long_value = p_arguments[1].get();
	
	int codons_length = codons_value->Count();
	
	bool integer_result = (long_value->Type() == EidosValueType::kValueInt);
	eidos_logical_t long_strings = (integer_result ? false : long_value->LogicalAtIndex(0, nullptr));
	
	if (integer_result)
	{
		int64_t long_intval = long_value->IntAtIndex(0, nullptr);
		
		if (long_intval != 0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires 'long' to be T, F, or 0." << EidosTerminate(nullptr);
	}
	
	if (codons_length == 1)
	{
		int64_t codon = codons_value->IntAtIndex(0, nullptr);
		
		if ((codon < 0) || (codon > 63))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
		
		if (integer_result)
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(codon2aa_int[codon]));
		}
		else
		{
			std::string &aa = (long_strings ? codon2aa_long[codon] : codon2aa_short[codon]);
		
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(aa));
		}
	}
	else
	{
		const int64_t *int_data = codons_value->IntVector()->data();
		
		if (integer_result)
		{
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(codons_length);
			
			for (int value_index = 0; value_index < codons_length; ++value_index)
			{
				int64_t codon = int_data[value_index];
				
				if ((codon < 0) || (codon > 63))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
				
				int_result->set_int_no_check(codon2aa_int[codon], value_index);
			}
			
			return EidosValue_SP(int_result);
		}
		else
		{
			EidosValue *paste_value = p_arguments[2].get();
			eidos_logical_t paste = paste_value->LogicalAtIndex(0, nullptr);
			
			if (paste)
			{
				if (long_strings && (codons_length > 0))
				{
					// pasting: "Aaa-Bbb-Ccc"
					EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
					std::string &aa_string = string_result->StringValue_Mutable();
					
					aa_string.resize(codons_length * 4 - 1);	// create space for all the amino acids we will generate, including hyphens
					
					char *aa_string_ptr = &aa_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
					
					for (int value_index = 0; value_index < codons_length; ++value_index)
					{
						int64_t codon = int_data[value_index];
						
						if ((codon < 0) || (codon > 63))
							EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
						
						std::string &aa = codon2aa_long[codon];
						char *codon_aa_ptr = &aa[0];	// data() returns a const pointer, but this is safe in C++11 and later
						
						if (value_index > 0)
							*(aa_string_ptr++) = '-';
						
						*(aa_string_ptr++) = codon_aa_ptr[0];
						*(aa_string_ptr++) = codon_aa_ptr[1];
						*(aa_string_ptr++) = codon_aa_ptr[2];
					}
					
					return EidosValue_SP(string_result);
				}
				else
				{
					// pasting: "ABC"
					EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
					std::string &aa_string = string_result->StringValue_Mutable();
					
					aa_string.resize(codons_length);	// create space for all the amino acids we will generate
					
					char *aa_string_ptr = &aa_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
					
					for (int value_index = 0; value_index < codons_length; ++value_index)
					{
						int64_t codon = int_data[value_index];
						
						if ((codon < 0) || (codon > 63))
							EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
						
						std::string &aa = codon2aa_short[codon];
						
						aa_string_ptr[value_index] = aa[0];
					}
					
					return EidosValue_SP(string_result);
				}
			}
			else
			{
				// no pasting: "A" "C" "C" or "Aaa" "Bbb" "Ccc"
				EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(codons_length);
				
				for (int value_index = 0; value_index < codons_length; ++value_index)
				{
					int64_t codon = int_data[value_index];
					
					if ((codon < 0) || (codon > 63))
						EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToAminoAcids): function codonsToAminoAcids() requires codons to be in [0, 63]." << EidosTerminate(nullptr);
					
					std::string &aa = (long_strings ? codon2aa_long[codon] : codon2aa_short[codon]);
					
					string_result->PushString(aa);
				}
				
				return EidosValue_SP(string_result);
			}
		}
	}
}

//	(integer)nucleotidesToCodons(is sequence)
EidosValue_SP SLiM_ExecuteFunction_nucleotidesToCodons(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *sequence_value = p_arguments[0].get();
	EidosValueType sequence_type = sequence_value->Type();
	int64_t sequence_count = sequence_value->Count();
	
	if (sequence_count == 1)
	{
		if (sequence_type == EidosValueType::kValueString)
		{
			// singleton string case
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::string &string_ref = sequence_value->IsSingleton() ? ((EidosValue_String_singleton *)sequence_value)->StringValue() : (*sequence_value->StringVector())[0];
			int64_t length = (int64_t)string_ref.length();
			
			if (length % 3 != 0)
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires the nucleotide sequence to be a multiple of three in length." << EidosTerminate(nullptr);
			
			int64_t length_3 = length / 3;
			
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length_3);
			
			for (int64_t value_index = 0; value_index < length_3; ++value_index)
			{
				int64_t codon_base = value_index * 3;
				
				int nuc1 = nuc_lookup[(int)(string_ref[codon_base])];
				int nuc2 = nuc_lookup[(int)(string_ref[codon_base + 1])];
				int nuc3 = nuc_lookup[(int)(string_ref[codon_base + 2])];
				
				if ((nuc1 > 3) || (nuc2 > 3) || (nuc3 > 3))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				int codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
				
				int_result->set_int_no_check(codon, value_index);
			}
			
			return EidosValue_SP(int_result);
		}
		else
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires the nucleotide sequence to be a multiple of three in length." << EidosTerminate(nullptr);
	}
	else
	{
		if (sequence_count % 3 != 0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires the nucleotide sequence to be a multiple of three in length." << EidosTerminate(nullptr);
		
		int64_t length_3 = sequence_count / 3;
		
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length_3);
		
		if (sequence_type == EidosValueType::kValueString)
		{
			// string vector case
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::vector<std::string> *string_vec = sequence_value->StringVector();
			
			for (int value_index = 0; value_index < length_3; ++value_index)
			{
				int64_t codon_base = value_index * 3;
				
				const std::string &nucstring1 = (*string_vec)[codon_base];
				const std::string &nucstring2 = (*string_vec)[codon_base + 1];
				const std::string &nucstring3 = (*string_vec)[codon_base + 2];
				
				if ((nucstring1.length() != 1) || (nucstring2.length() != 1) || (nucstring3.length() != 1))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				int nuc1 = nuc_lookup[(int)(nucstring1[0])];
				int nuc2 = nuc_lookup[(int)(nucstring2[0])];
				int nuc3 = nuc_lookup[(int)(nucstring3[0])];
				
				if ((nuc1 > 3) || (nuc2 > 3) || (nuc3 > 3))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				int codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
				
				int_result->set_int_no_check(codon, value_index);
			}
		}
		else // sequence_type == EidosValueType::kValueInt
		{
			// int vector case
			const int64_t *int_data = sequence_value->IntVector()->data();
			
			for (int value_index = 0; value_index < length_3; ++value_index)
			{
				int64_t codon_base = value_index * 3;
				
				int64_t nuc1 = int_data[codon_base];
				int64_t nuc2 = int_data[codon_base + 1];
				int64_t nuc3 = int_data[codon_base + 2];
				
				if ((nuc1 < 0) || (nuc1 > 3) || (nuc2 < 0) || (nuc2 > 3) || (nuc3 < 0) || (nuc3 > 3))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_nucleotidesToCodons): function nucleotidesToCodons() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
				
				int64_t codon = nuc1 * 16 + nuc2 * 4 + nuc3;	// 0..63
				
				int_result->set_int_no_check(codon, value_index);
			}
		}
		
		return EidosValue_SP(int_result);
	}
}

static void CountNucleotides(EidosValue *sequence_value, int64_t *total_ACGT, const char *function_name)
{
	EidosValueType sequence_type = sequence_value->Type();
	int sequence_count = sequence_value->Count();
	
	if (sequence_count == 1)
	{
		// Singleton case
		if (sequence_type == EidosValueType::kValueInt)
		{
			uint64_t nuc = sequence_value->IntAtIndex(0, nullptr);
			
			if (nuc > 3)
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
			
			total_ACGT[nuc]++;
		}
		else // sequence_type == EidosValueType::kValueString
		{
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::string &string_ref = sequence_value->IsSingleton() ? ((EidosValue_String_singleton *)sequence_value)->StringValue() : (*sequence_value->StringVector())[0];
			std::size_t length = string_ref.length();
			
			for (std::size_t i = 0; i < length; ++i)
			{
				char nuc_char = string_ref[i];
				uint8_t nuc_index = nuc_lookup[(int)(nuc_char)];
				
				if (nuc_index > 3)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				total_ACGT[nuc_index]++;
			}
		}
	}
	else
	{
		// Vector case, optimized for speed
		if (sequence_type == EidosValueType::kValueInt)
		{
			const int64_t *int_data = sequence_value->IntVector()->data();
			
			for (int value_index = 0; value_index < sequence_count; ++value_index)
			{
				uint64_t nuc = int_data[value_index];
				
				if (nuc > 3)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
				
				total_ACGT[nuc]++;
			}
		}
		else // sequence_type == EidosValueType::kValueString
		{
			uint8_t *nuc_lookup = NucleotideArray::NucleotideCharToIntLookup();
			const std::vector<std::string> *string_vec = sequence_value->StringVector();
			
			for (int value_index = 0; value_index < sequence_count; ++value_index)
			{
				const std::string &nuc_string = (*string_vec)[value_index];
				
				if (nuc_string.length() != 1)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				char nuc_char = nuc_string[0];
				uint8_t nuc_index = nuc_lookup[(int)(nuc_char)];
				
				if (nuc_index > 3)
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
				
				total_ACGT[nuc_index]++;
			}
		}
	}
}

//	(float)mm16To256(float mutationMatrix16)
EidosValue_SP SLiM_ExecuteFunction_mm16To256(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *mutationMatrix16_value = p_arguments[0].get();
	
	if (mutationMatrix16_value->Count() != 16)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mm16To256): function mm16To256() requires mutationMatrix16 to be of length 16." << EidosTerminate(nullptr);
	if ((mutationMatrix16_value->DimensionCount() != 2) || (mutationMatrix16_value->Dimensions()[0] != 4) || (mutationMatrix16_value->Dimensions()[1] != 4))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mm16To256): function mm16To256() requires mutationMatrix16 to be a 4x4 matrix." << EidosTerminate(nullptr);
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(256);
	
	for (int i = 0; i < 256; ++i)
	{
		int ancestral_nucleotide = ((i / 4) % 4);
		int derived_nucleotide = (i / 64);
		double value = mutationMatrix16_value->FloatAtIndex(ancestral_nucleotide + derived_nucleotide * 4, nullptr);
		
		float_result->set_float_no_check(value, i);
	}
	
	const int64_t dims[2] = {64, 4};
	float_result->SetDimensions(2, dims);
	
	return EidosValue_SP(float_result);
}

//	(float)mmJukesCantor(float$ alpha)
EidosValue_SP SLiM_ExecuteFunction_mmJukesCantor(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *alpha_value = p_arguments[0].get();
	double alpha = alpha_value->FloatAtIndex(0, nullptr);
	
	if (alpha < 0.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmJukesCantor): function mmJukesCantor() requires alpha >= 0.0." << EidosTerminate(nullptr);
	if (3 * alpha > 1.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmJukesCantor): function mmJukesCantor() requires 3 * alpha <= 1.0." << EidosTerminate(nullptr);
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(16);
	
	float_result->set_float_no_check(0.0, 0);
	float_result->set_float_no_check(alpha, 1);
	float_result->set_float_no_check(alpha, 2);
	float_result->set_float_no_check(alpha, 3);
	
	float_result->set_float_no_check(alpha, 4);
	float_result->set_float_no_check(0.0, 5);
	float_result->set_float_no_check(alpha, 6);
	float_result->set_float_no_check(alpha, 7);
	
	float_result->set_float_no_check(alpha, 8);
	float_result->set_float_no_check(alpha, 9);
	float_result->set_float_no_check(0.0, 10);
	float_result->set_float_no_check(alpha, 11);
	
	float_result->set_float_no_check(alpha, 12);
	float_result->set_float_no_check(alpha, 13);
	float_result->set_float_no_check(alpha, 14);
	float_result->set_float_no_check(0.0, 15);
	
	const int64_t dims[2] = {4, 4};
	float_result->SetDimensions(2, dims);
	
	return EidosValue_SP(float_result);
}

//	(float)mmKimura(float$ alpha, float$ beta)
EidosValue_SP SLiM_ExecuteFunction_mmKimura(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *alpha_value = p_arguments[0].get();
	EidosValue *beta_value = p_arguments[1].get();
	
	double alpha = alpha_value->FloatAtIndex(0, nullptr);
	double beta = beta_value->FloatAtIndex(0, nullptr);
	
	if ((alpha < 0.0) || (alpha > 1.0))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires alpha to be in [0.0, 1.0]." << EidosTerminate(nullptr);
	if ((beta < 0.0) || (beta > 0.5))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires beta to be in [0.0, 0.5]." << EidosTerminate(nullptr);
	if (alpha + 2 * beta > 1.0)
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_mmKimura): function mmKimura() requires alpha + 2 * beta to be <= 1.0." << EidosTerminate(nullptr);
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(16);
	
	float_result->set_float_no_check(0.0, 0);
	float_result->set_float_no_check(beta, 1);
	float_result->set_float_no_check(alpha, 2);
	float_result->set_float_no_check(beta, 3);
	
	float_result->set_float_no_check(beta, 4);
	float_result->set_float_no_check(0.0, 5);
	float_result->set_float_no_check(beta, 6);
	float_result->set_float_no_check(alpha, 7);
	
	float_result->set_float_no_check(alpha, 8);
	float_result->set_float_no_check(beta, 9);
	float_result->set_float_no_check(0.0, 10);
	float_result->set_float_no_check(beta, 11);
	
	float_result->set_float_no_check(beta, 12);
	float_result->set_float_no_check(alpha, 13);
	float_result->set_float_no_check(beta, 14);
	float_result->set_float_no_check(0.0, 15);
	
	const int64_t dims[2] = {4, 4};
	float_result->SetDimensions(2, dims);
	
	return EidosValue_SP(float_result);
}

//	(integer)nucleotideCounts(is sequence)
EidosValue_SP SLiM_ExecuteFunction_nucleotideCounts(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *sequence_value = p_arguments[0].get();
	int64_t total_ACGT[4] = {0, 0, 0, 0};
	
	CountNucleotides(sequence_value, total_ACGT, "nucleotideCounts");
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(4);
	
	int_result->set_int_no_check(total_ACGT[0], 0);
	int_result->set_int_no_check(total_ACGT[1], 1);
	int_result->set_int_no_check(total_ACGT[2], 2);
	int_result->set_int_no_check(total_ACGT[3], 3);
	
	return EidosValue_SP(int_result);
}

//	(float)nucleotideFrequencies(is sequence)
EidosValue_SP SLiM_ExecuteFunction_nucleotideFrequencies(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *sequence_value = p_arguments[0].get();
	int64_t total_ACGT[4] = {0, 0, 0, 0};
	
	CountNucleotides(sequence_value, total_ACGT, "nucleotideFrequencies");
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(4);
	double total = total_ACGT[0] + total_ACGT[1] + total_ACGT[2] + total_ACGT[3];
	
	float_result->set_float_no_check(total_ACGT[0] / total, 0);
	float_result->set_float_no_check(total_ACGT[1] / total, 1);
	float_result->set_float_no_check(total_ACGT[2] / total, 2);
	float_result->set_float_no_check(total_ACGT[3] / total, 3);
	
	return EidosValue_SP(float_result);
}

//	(is)randomNucleotides(i$ length, [Nif basis = NULL], [s$ format = "string"])
EidosValue_SP SLiM_ExecuteFunction_randomNucleotides(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	EidosValue *basis_value = p_arguments[1].get();
	EidosValue *format_value = p_arguments[2].get();
	
	// Get the sequence length to generate
	int64_t length = length_value->IntAtIndex(0, nullptr);
	
	if ((length < 0) || (length > 2000000000L))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires length to be in [0, 2e9]." << EidosTerminate(nullptr);
	
	// Figure out the probability threshold for each base
	double pA = 0.25, pC = 0.25, pG = 0.25, pT = 0.25;
	
	if (basis_value->Type() != EidosValueType::kValueNULL)
	{
		if (basis_value->Count() != 4)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires basis to be either NULL, or an integer or float vector of length 4 (with relative probabilities for A/C/G/T)." << EidosTerminate(nullptr);
		
		pA = basis_value->FloatAtIndex(0, nullptr);
		pC = basis_value->FloatAtIndex(1, nullptr);
		pG = basis_value->FloatAtIndex(2, nullptr);
		pT = basis_value->FloatAtIndex(3, nullptr);
		
		if (!std::isfinite(pA) || !std::isfinite(pC) || !std::isfinite(pG) || !std::isfinite(pT) || (pA < 0.0) || (pC < 0.0) || (pG < 0.0) || (pT < 0.0))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires basis values to be finite and >= 0.0." << EidosTerminate(nullptr);
		
		double sum = pA + pC + pG + pT;
		
		if (sum <= 0.0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires at least one basis value to be > 0.0." << EidosTerminate(nullptr);
		
		// Normalize to probabilities
		pA = pA / sum;
		pC = pC / sum;
		pG = pG / sum;
		pT = pT / sum;
	}
	
	// Convert probabilities to thresholds
	pT += pA + pC + pG;		// should be 1.0; not used
	pG += pA + pC;
	pC += pA;
	
	// Generate a result in the requested format
	std::string &&format = format_value->StringAtIndex(0, nullptr);
	
	if ((format != "string") && (format != "char") && (format != "integer"))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomNucleotides): function randomNucleotides() requires a format of 'string', 'char', or 'integer'." << EidosTerminate();
	
	if (length == 0)
	{
		if (format == "integer")	return gStaticEidosValue_Integer_ZeroVec;
		else						return gStaticEidosValue_String_ZeroVec;
	}
	
	if (length == 1)
	{
		// Handle the singleton case separately for speed
		double runif = Eidos_rng_uniform(EIDOS_GSL_RNG);
		
		if (format == "integer")
		{
			if (runif < pA)			return gStaticEidosValue_Integer0;
			else if (runif < pC)	return gStaticEidosValue_Integer1;
			else if (runif < pG)	return gStaticEidosValue_Integer2;
			else /* (runif < pT) */	return gStaticEidosValue_Integer3;
		}
		else	// "string", "char"
		{
			if (runif < pA)			return gStaticEidosValue_StringA;
			else if (runif < pC)	return gStaticEidosValue_StringC;
			else if (runif < pG)	return gStaticEidosValue_StringG;
			else /* (runif < pT) */	return gStaticEidosValue_StringT;
		}
	}
	
	if (format == "char")
	{
		// return a vector of one-character strings, "T" "A" "T" "A"
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)length);
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			double runif = Eidos_rng_uniform(EIDOS_GSL_RNG);
			
			if (runif < pA)			string_result->PushString(gStr_A);
			else if (runif < pC)	string_result->PushString(gStr_C);
			else if (runif < pG)	string_result->PushString(gStr_G);
			else /* (runif < pT) */	string_result->PushString(gStr_T);
		}
		
		return EidosValue_SP(string_result);
	}
	else if (format == "integer")
	{
		// return a vector of integers, 3 0 3 0
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length);
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			double runif = Eidos_rng_uniform(EIDOS_GSL_RNG);
			
			if (runif < pA)			int_result->set_int_no_check(0, value_index);
			else if (runif < pC)	int_result->set_int_no_check(1, value_index);
			else if (runif < pG)	int_result->set_int_no_check(2, value_index);
			else /* (runif < pT) */	int_result->set_int_no_check(3, value_index);
		}
		
		return EidosValue_SP(int_result);
	}
	else if (format == "string")
	{
		// return a singleton string for the whole sequence, "TATA"; we munge the std::string inside the EidosValue to avoid memory copying, very naughty
		EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		std::string &nuc_string = string_result->StringValue_Mutable();
		
		nuc_string.resize(length);	// create space for all the nucleotides we will generate
		
		char *nuc_string_ptr = &nuc_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			double runif = Eidos_rng_uniform(EIDOS_GSL_RNG);
			
			if (runif < pA)			nuc_string_ptr[value_index] = 'A';
			else if (runif < pC)	nuc_string_ptr[value_index] = 'C';
			else if (runif < pG)	nuc_string_ptr[value_index] = 'G';
			else /* (runif < pT) */	nuc_string_ptr[value_index] = 'T';
		}
		
		return EidosValue_SP(string_result);
	}
	
	// CODE COVERAGE: This is dead code
	return result_SP;
}

// (is)codonsToNucleotides(integer codons, [string$ format = "string"])
EidosValue_SP SLiM_ExecuteFunction_codonsToNucleotides(const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *codons_value = p_arguments[0].get();
	EidosValue *format_value = p_arguments[1].get();
	
	int codons_length = codons_value->Count();
	int length = codons_length * 3;
	std::string &&format = format_value->StringAtIndex(0, nullptr);
	
	if (format == "char")
	{
		// return a vector of one-character strings, "T" "A" "T" "A" "C" "G"
		EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)length);
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_value->IntAtIndex(codon_index, nullptr);
			
			if ((codon < 0) || (codon > 63))
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires codon values to be in [0,63]." << EidosTerminate();
			
			int nuc1 = codon >> 4;
			int nuc2 = (codon >> 2) & 0x03;
			int nuc3 = codon & 0x03;
			
			switch (nuc1) {
				case 0: string_result->PushString(gStr_A); break;
				case 1: string_result->PushString(gStr_C); break;
				case 2: string_result->PushString(gStr_G); break;
				case 3: string_result->PushString(gStr_T); break;
			}
			switch (nuc2) {
				case 0: string_result->PushString(gStr_A); break;
				case 1: string_result->PushString(gStr_C); break;
				case 2: string_result->PushString(gStr_G); break;
				case 3: string_result->PushString(gStr_T); break;
			}
			switch (nuc3) {
				case 0: string_result->PushString(gStr_A); break;
				case 1: string_result->PushString(gStr_C); break;
				case 2: string_result->PushString(gStr_G); break;
				case 3: string_result->PushString(gStr_T); break;
			}
		}
		
		return EidosValue_SP(string_result);
	}
	else if (format == "integer")
	{
		// return a vector of integers, 3 0 3 0 1 2
		EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize((int)length);
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_value->IntAtIndex(codon_index, nullptr);
			
			if ((codon < 0) || (codon > 63))
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires codon values to be in [0,63]." << EidosTerminate();
			
			int nuc1 = codon >> 4;
			int nuc2 = (codon >> 2) & 0x03;
			int nuc3 = codon & 0x03;
			
			int_result->set_int_no_check(nuc1, codon_index * 3);
			int_result->set_int_no_check(nuc2, codon_index * 3 + 1);
			int_result->set_int_no_check(nuc3, codon_index * 3 + 2);
		}
		
		return EidosValue_SP(int_result);
	}
	else if (format == "string")
	{
		// return a singleton string for the whole sequence, "TATACG"; we munge the std::string inside the EidosValue to avoid memory copying, very naughty
		EidosValue_String_singleton *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		std::string &nuc_string = string_result->StringValue_Mutable();
		
		nuc_string.resize(length);	// create space for all the nucleotides we will generate
		
		char *nuc_string_ptr = &nuc_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
		static const char nuc_chars[4] = {'A', 'C', 'G', 'T'};
		
		for (int codon_index = 0; codon_index < codons_length; ++codon_index)
		{
			int codon = (int)codons_value->IntAtIndex(codon_index, nullptr);
			
			if ((codon < 0) || (codon > 63))
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires codon values to be in [0,63]." << EidosTerminate();
			
			int nuc1 = codon >> 4;
			int nuc2 = (codon >> 2) & 0x03;
			int nuc3 = codon & 0x03;
			
			nuc_string_ptr[codon_index * 3] = nuc_chars[nuc1];
			nuc_string_ptr[codon_index * 3 + 1] = nuc_chars[nuc2];
			nuc_string_ptr[codon_index * 3 + 2] = nuc_chars[nuc3];
		}
		
		return EidosValue_SP(string_result);
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_codonsToNucleotides): function codonsToNucleotides() requires a format of 'string', 'char', or 'integer'." << EidosTerminate();
	}
}


































