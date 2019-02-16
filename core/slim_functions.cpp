//
//  slim_functions.cpp
//  SLiM
//
//  Created by Ben Haller on 2/15/19.
//  Copyright (c) 2014-2019 Philipp Messer.  All rights reserved.
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
#include "slim_global.h"
#include "slim_sim.h"
#include "eidos_rng.h"

#include <string>


const std::vector<EidosFunctionSignature_SP> *SLiMSim::SLiMFunctionSignatures(void)
{
	// Allocate our own EidosFunctionSignature objects
	static std::vector<EidosFunctionSignature_SP> sim_func_signatures_;
	
	if (!sim_func_signatures_.size())
	{
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideCounts", SLiM_ExecuteFunction_nucleotideCounts, kEidosValueMaskInt, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("nucleotideFrequencies", SLiM_ExecuteFunction_nucleotideFrequencies, kEidosValueMaskFloat, "SLiM"))->AddIntString("sequence"));
		sim_func_signatures_.emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature("randomSequence", SLiM_ExecuteFunction_randomSequence, kEidosValueMaskInt | kEidosValueMaskString, "SLiM"))->AddInt_S("length")->AddFloat_ON("basis", gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string"))));
	}
	
	return &sim_func_signatures_;
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
			int64_t nuc = sequence_value->IntAtIndex(0, nullptr);
			
			if ((nuc < 0) || (nuc > 3))
				EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
			
			total_ACGT[nuc]++;
		}
		else // sequence_type == EidosValueType::kValueString
		{
			const std::string &string_ref = sequence_value->IsSingleton() ? ((EidosValue_String_singleton *)sequence_value)->StringValue() : (*sequence_value->StringVector())[0];
			std::size_t length = string_ref.length();
			
			for (std::size_t i = 0; i < length; ++i)
			{
				char nuc = string_ref[i];
				
				if (nuc == 'A')			total_ACGT[0]++;
				else if (nuc == 'C')	total_ACGT[1]++;
				else if (nuc == 'G')	total_ACGT[2]++;
				else if (nuc == 'T')	total_ACGT[3]++;
				else EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
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
				int64_t nuc = int_data[value_index];
				
				if ((nuc < 0) || (nuc > 3))
					EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires integer sequence values to be in [0,3]." << EidosTerminate(nullptr);
				
				total_ACGT[nuc]++;
			}
		}
		else // sequence_type == EidosValueType::kValueString
		{
			const std::vector<std::string> *string_vec = sequence_value->StringVector();
			
			for (int value_index = 0; value_index < sequence_count; ++value_index)
			{
				const std::string &nuc = (*string_vec)[value_index];
				
				if (nuc == "A")			total_ACGT[0]++;
				else if (nuc == "C")	total_ACGT[1]++;
				else if (nuc == "G")	total_ACGT[2]++;
				else if (nuc == "T")	total_ACGT[3]++;
				else EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_" << function_name << "): function " << function_name << "() requires string sequence values to be 'A', 'C', 'G', or 'T'." << EidosTerminate(nullptr);
			}
		}
	}
}

//	(integer)nucleotideCounts(is sequence)
EidosValue_SP SLiM_ExecuteFunction_nucleotideCounts(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
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
EidosValue_SP SLiM_ExecuteFunction_nucleotideFrequencies(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue *sequence_value = p_arguments[0].get();
	int64_t total_ACGT[4] = {0, 0, 0, 0};
	
	CountNucleotides(sequence_value, total_ACGT, "nucleotideCounts");
	
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(4);
	double total = total_ACGT[0] + total_ACGT[1] + total_ACGT[2] + total_ACGT[3];
	
	float_result->set_float_no_check(total_ACGT[0] / total, 0);
	float_result->set_float_no_check(total_ACGT[1] / total, 1);
	float_result->set_float_no_check(total_ACGT[2] / total, 2);
	float_result->set_float_no_check(total_ACGT[3] / total, 3);
	
	return EidosValue_SP(float_result);
}

//	(is)randomSequence(i$ length, [Nf basis = NULL], [s$ format = "string"])
EidosValue_SP SLiM_ExecuteFunction_randomSequence(const EidosValue_SP *const p_arguments, __attribute__((unused)) int p_argument_count, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	EidosValue_SP result_SP(nullptr);
	
	EidosValue *length_value = p_arguments[0].get();
	EidosValue *basis_value = p_arguments[1].get();
	EidosValue *format_value = p_arguments[2].get();
	
	// Get the sequence length to generate
	int64_t length = length_value->IntAtIndex(0, nullptr);
	
	if ((length <= 0) || (length > 2000000000L))
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomSequence): function randomSequence() requires length to be in [1, 2e9]." << EidosTerminate(nullptr);
	
	// Figure out the probability threshold for each base
	double pA = 0.25, pC = 0.25, pG = 0.25, pT = 0.25;
	
	if (basis_value->Type() != EidosValueType::kValueNULL)
	{
		if (basis_value->Count() != 4)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomSequence): function randomSequence() requires basis to be either NULL, or a float vector of length 4 (with probabilities for A/C/G/T)." << EidosTerminate(nullptr);
		
		pA = basis_value->FloatAtIndex(0, nullptr);
		pC = basis_value->FloatAtIndex(1, nullptr);
		pG = basis_value->FloatAtIndex(2, nullptr);
		pT = basis_value->FloatAtIndex(3, nullptr);
		
		if (!std::isfinite(pA) || !std::isfinite(pC) || !std::isfinite(pG) || !std::isfinite(pT) || (pA < 0.0) || (pC < 0.0) || (pG < 0.0) || (pT < 0.0))
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomSequence): function randomSequence() requires basis values to be finite and >= 0.0." << EidosTerminate(nullptr);
		
		double sum = pA + pC + pG + pT;
		
		if (sum <= 0.0)
			EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomSequence): function randomSequence() requires at least one basis value to be > 0.0." << EidosTerminate(nullptr);
		
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
	else
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomSequence): function randomSequence() requires a format of 'string', 'char', or 'integer'." << EidosTerminate();
	
	return result_SP;
}


































