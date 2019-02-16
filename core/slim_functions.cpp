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
#include "eidos_rng.h"


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
		
		if (!isfinite(pA) || !isfinite(pC) || !isfinite(pG) || !isfinite(pT) || (pA < 0.0) || (pC < 0.0) || (pG < 0.0) || (pT < 0.0))
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
		
		for (int value_index = 0; value_index < length; ++value_index)
		{
			double runif = Eidos_rng_uniform(EIDOS_GSL_RNG);
			
			if (runif < pA)			nuc_string.append(1, 'A');
			else if (runif < pC)	nuc_string.append(1, 'C');
			else if (runif < pG)	nuc_string.append(1, 'G');
			else /* (runif < pT) */	nuc_string.append(1, 'T');
		}
		
		return EidosValue_SP(string_result);
	}
	else
		EIDOS_TERMINATION << "ERROR (SLiM_ExecuteFunction_randomSequence): function randomSequence() requires a format of 'string', 'char', or 'integer'." << EidosTerminate();
	
	return result_SP;
}


































