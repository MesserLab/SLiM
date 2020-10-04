//
//  mutation_type.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
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


#include "mutation_type.h"
#include "eidos_rng.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_eidos_block.h"
#include "slim_sim.h"	// we need to tell the simulation if a DFE is set to non-neutral...

#include <iostream>
#include <sstream>
#include <algorithm>


// stream output for enumerations
std::ostream& operator<<(std::ostream& p_out, DFEType p_dfe_type)
{
	switch (p_dfe_type)
	{
		case DFEType::kFixed:			p_out << gStr_f;		break;
		case DFEType::kGamma:			p_out << gStr_g;		break;
		case DFEType::kExponential:		p_out << gStr_e;		break;
		case DFEType::kNormal:			p_out << gEidosStr_n;	break;
		case DFEType::kWeibull:			p_out << gStr_w;		break;
		case DFEType::kScript:			p_out << gEidosStr_s;	break;
	}
	
	return p_out;
}


#pragma mark -
#pragma mark MutationType
#pragma mark -

#ifdef SLIMGUI
MutationType::MutationType(SLiMSim &p_sim, slim_objectid_t p_mutation_type_id, double p_dominance_coeff, bool p_nuc_based, DFEType p_dfe_type, std::vector<double> p_dfe_parameters, std::vector<std::string> p_dfe_strings, int p_mutation_type_index) :
#else
MutationType::MutationType(SLiMSim &p_sim, slim_objectid_t p_mutation_type_id, double p_dominance_coeff, bool p_nuc_based, DFEType p_dfe_type, std::vector<double> p_dfe_parameters, std::vector<std::string> p_dfe_strings) :
#endif
self_symbol_(Eidos_GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('m', p_mutation_type_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_MutationType_Class))),
	sim_(p_sim), mutation_type_id_(p_mutation_type_id), dominance_coeff_(static_cast<slim_selcoeff_t>(p_dominance_coeff)), dominance_coeff_changed_(false), dfe_type_(p_dfe_type), dfe_parameters_(p_dfe_parameters), dfe_strings_(p_dfe_strings), nucleotide_based_(p_nuc_based), convert_to_substitution_(false), stack_policy_(MutationStackPolicy::kStack), stack_group_(p_mutation_type_id), cached_dfe_script_(nullptr)
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	, muttype_registry_call_count_(0), keeping_muttype_registry_(false)
#endif
#ifdef SLIMGUI
	, mutation_type_index_(p_mutation_type_index)
#endif
{
	// In WF models, convertToSubstitution defaults to T; in nonWF models it defaults to F as specified above
	if (sim_.ModelType() == SLiMModelType::kModelTypeWF)
		convert_to_substitution_ = true;
	
	if ((dfe_parameters_.size() == 0) && (dfe_strings_.size() == 0))
		EIDOS_TERMINATION << "ERROR (MutationType::MutationType): invalid mutation type parameters." << EidosTerminate();
	// intentionally no bounds checks for DFE parameters; the count of DFE parameters is checked prior to construction
	// intentionally no bounds check for dominance_coeff_
	
	// determine whether this mutation type is initially pure neutral; note that this flag will be
	// cleared if any mutation of this type has its selection coefficient changed
	// note also that we do not set SLiMSim.pure_neutral_ here; we wait until this muttype is used
	all_pure_neutral_DFE_ = ((dfe_type_ == DFEType::kFixed) && (dfe_parameters_[0] == 0.0));
	
	// Nucleotide-based mutations use a special stacking group, -1, and always use stacking policy "l"
	if (p_nuc_based)
	{
		stack_policy_ = MutationStackPolicy::kKeepLast;
		stack_group_ = -1;
	}
	
	// The fact that we have been created means that stacking policy has changed and needs to be checked
	sim_.MutationStackPolicyChanged();
}

MutationType::~MutationType(void)
{
	delete cached_dfe_script_;
	cached_dfe_script_ = nullptr;
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	if (keeping_muttype_registry_)
	{
		muttype_registry_.clear();
		keeping_muttype_registry_ = false;
	}
#endif
}

void MutationType::ParseDFEParameters(std::string &p_dfe_type_string, const EidosValue_SP *const p_arguments, int p_argument_count, DFEType *p_dfe_type, std::vector<double> *p_dfe_parameters, std::vector<std::string> *p_dfe_strings)
{
	// First we figure out the DFE type from p_dfe_type_string, and set up expectations based on that
	int expected_dfe_param_count = 0;
	bool params_are_numeric = true;
	
	if (p_dfe_type_string.compare(gStr_f) == 0)
	{
		*p_dfe_type = DFEType::kFixed;
		expected_dfe_param_count = 1;
	}
	else if (p_dfe_type_string.compare(gStr_g) == 0)
	{
		*p_dfe_type = DFEType::kGamma;
		expected_dfe_param_count = 2;
	}
	else if (p_dfe_type_string.compare(gStr_e) == 0)
	{
		*p_dfe_type = DFEType::kExponential;
		expected_dfe_param_count = 1;
	}
	else if (p_dfe_type_string.compare(gEidosStr_n) == 0)
	{
		*p_dfe_type = DFEType::kNormal;
		expected_dfe_param_count = 2;
	}
	else if (p_dfe_type_string.compare(gStr_w) == 0)
	{
		*p_dfe_type = DFEType::kWeibull;
		expected_dfe_param_count = 2;
	}
	else if (p_dfe_type_string.compare(gEidosStr_s) == 0)
	{
		*p_dfe_type = DFEType::kScript;
		expected_dfe_param_count = 1;
		params_are_numeric = false;
	}
	else
		EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): distribution type \"" << p_dfe_type_string << "\" must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"." << EidosTerminate();
	
	if (p_argument_count != expected_dfe_param_count)
		EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): distribution type \"" << *p_dfe_type << "\" requires exactly " << expected_dfe_param_count << " DFE parameter" << (expected_dfe_param_count == 1 ? "" : "s") << "." << EidosTerminate();
	
	// Next we extract the parameter values, checking their types in accordance with params_are_numeric
	p_dfe_parameters->clear();
	p_dfe_strings->clear();
	
	for (int dfe_param_index = 0; dfe_param_index < expected_dfe_param_count; ++dfe_param_index)
	{
		EidosValue *dfe_param_value = p_arguments[dfe_param_index].get();
		EidosValueType dfe_param_type = dfe_param_value->Type();
		
		if (params_are_numeric)
		{
			if ((dfe_param_type != EidosValueType::kValueFloat) && (dfe_param_type != EidosValueType::kValueInt))
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): the parameters for a DFE of type \"" << *p_dfe_type << "\" must be of type numeric (integer or float)." << EidosTerminate();
			
			p_dfe_parameters->emplace_back(dfe_param_value->FloatAtIndex(0, nullptr));
		}
		else
		{
			if (dfe_param_type != EidosValueType::kValueString)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): the parameters for a DFE of type \"" << *p_dfe_type << "\" must be of type string." << EidosTerminate();
			
			p_dfe_strings->emplace_back(dfe_param_value->StringAtIndex(0, nullptr));
		}
	}
	
	// Finally, we bounds-check the DFE parameters in the cases where there is a hard bound
	switch (*p_dfe_type)
	{
		case DFEType::kFixed:
			// no limits on fixed DFEs; we could check that s >= -1, but that assumes that the selection coefficients are being used as selection coefficients
			break;
		case DFEType::kGamma:
			// mean is unrestricted, shape parameter must be >0 (officially mean > 0, but we allow mean <= 0 and the GSL handles it)
			if ((*p_dfe_parameters)[1] <= 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): a DFE of type \"g\" must have a shape parameter > 0." << EidosTerminate();
			break;
		case DFEType::kExponential:
			// no limits on exponential DFEs (officially scale > 0, but we allow scale <= 0 and the GSL handles it)
			break;
		case DFEType::kNormal:
			// mean is unrestricted, sd parameter must be >= 0
			if ((*p_dfe_parameters)[1] < 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): a DFE of type \"n\" must have a standard deviation parameter >= 0." << EidosTerminate();
			break;
		case DFEType::kWeibull:
			// scale and shape must both be > 0
			if ((*p_dfe_parameters)[0] <= 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): a DFE of type \"w\" must have a scale parameter > 0." << EidosTerminate();
			if ((*p_dfe_parameters)[1] <= 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDFEParameters): a DFE of type \"w\" must have a shape parameter > 0." << EidosTerminate();
			break;
		case DFEType::kScript:
			// no limits on script here; the script is checked when it gets tokenized/parsed/executed
			break;
	}
}

double MutationType::DrawSelectionCoefficient(void) const
{
	switch (dfe_type_)
	{
		case DFEType::kFixed:			return dfe_parameters_[0];
		case DFEType::kGamma:			return gsl_ran_gamma(EIDOS_GSL_RNG, dfe_parameters_[1], dfe_parameters_[0] / dfe_parameters_[1]);
		case DFEType::kExponential:		return gsl_ran_exponential(EIDOS_GSL_RNG, dfe_parameters_[0]);
		case DFEType::kNormal:			return gsl_ran_gaussian(EIDOS_GSL_RNG, dfe_parameters_[1]) + dfe_parameters_[0];
		case DFEType::kWeibull:			return gsl_ran_weibull(EIDOS_GSL_RNG, dfe_parameters_[0], dfe_parameters_[1]);
			
		case DFEType::kScript:
		{
			// We have a script string that we need to execute, and it will return a float or integer to us.  This
			// is basically a lambda call, so the code here is parallel to the executeLambda() code in many ways.
			double sel_coeff;
			
			// Errors in lambdas should be reported for the lambda script, not for the calling script,
			// if possible.  In the GUI this does not work well, however; there, errors should be
			// reported as occurring in the call to executeLambda().  Here we save off the current
			// error context and set up the error context for reporting errors inside the lambda,
			// in case that is possible; see how exceptions are handled below.
			int error_start_save = gEidosCharacterStartOfError;
			int error_end_save = gEidosCharacterEndOfError;
			int error_start_save_UTF16 = gEidosCharacterStartOfErrorUTF16;
			int error_end_save_UTF16 = gEidosCharacterEndOfErrorUTF16;
			EidosScript *current_script_save = gEidosCurrentScript;
			bool executing_runtime_script_save = gEidosExecutingRuntimeScript;
			
			// We try to do tokenization and parsing once per script, by caching the script
			if (!cached_dfe_script_)
			{
				std::string script_string = dfe_strings_[0];
				cached_dfe_script_ = new EidosScript(script_string);
				
				gEidosCharacterStartOfError = -1;
				gEidosCharacterEndOfError = -1;
				gEidosCharacterStartOfErrorUTF16 = -1;
				gEidosCharacterEndOfErrorUTF16 = -1;
				gEidosCurrentScript = cached_dfe_script_;
				gEidosExecutingRuntimeScript = true;
				
				try
				{
					cached_dfe_script_->Tokenize();
					cached_dfe_script_->ParseInterpreterBlockToAST(false);
				}
				catch (...)
				{
					if (gEidosTerminateThrows)
					{
						gEidosCharacterStartOfError = error_start_save;
						gEidosCharacterEndOfError = error_end_save;
						gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
						gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
						gEidosCurrentScript = current_script_save;
						gEidosExecutingRuntimeScript = executing_runtime_script_save;
					}
					
					delete cached_dfe_script_;
					cached_dfe_script_ = nullptr;
					
					EIDOS_TERMINATION << "ERROR (MutationType::DrawSelectionCoefficient): tokenize/parse error in type 's' DFE callback script." << EidosTerminate(nullptr);
				}
			}
			
			// Execute inside try/catch so we can handle errors well
			gEidosCharacterStartOfError = -1;
			gEidosCharacterEndOfError = -1;
			gEidosCharacterStartOfErrorUTF16 = -1;
			gEidosCharacterEndOfErrorUTF16 = -1;
			gEidosCurrentScript = cached_dfe_script_;
			gEidosExecutingRuntimeScript = true;
			
			try
			{
				EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &sim_.SymbolTable());
				EidosFunctionMap &function_map = sim_.FunctionMap();
				EidosInterpreter interpreter(*cached_dfe_script_, client_symbols, function_map, nullptr);
				
				EidosValue_SP result_SP = interpreter.EvaluateInterpreterBlock(false, true);	// do not print output, return the last statement value
				EidosValue *result = result_SP.get();
				EidosValueType result_type = result->Type();
				int result_count = result->Count();
				
				if ((result_type == EidosValueType::kValueFloat) && (result_count == 1))
					sel_coeff = result->FloatAtIndex(0, nullptr);
				else if ((result_type == EidosValueType::kValueInt) && (result_count == 1))
					sel_coeff = result->IntAtIndex(0, nullptr);
				else
					EIDOS_TERMINATION << "ERROR (MutationType::DrawSelectionCoefficient): type 's' DFE callbacks must provide a singleton float or integer return value." << EidosTerminate(nullptr);
				
				// Output generated by the interpreter goes to our output stream
				interpreter.FlushExecutionOutputToStream(SLIM_OUTSTREAM);
			}
			catch (...)
			{
				// If exceptions throw, then we want to set up the error information to highlight the
				// executeLambda() that failed, since we can't highlight the actual error.  (If exceptions
				// don't throw, this catch block will never be hit; exit() will already have been called
				// and the error will have been reported from the context of the lambda script string.)
				if (gEidosTerminateThrows)
				{
					gEidosCharacterStartOfError = error_start_save;
					gEidosCharacterEndOfError = error_end_save;
					gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
					gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
					gEidosCurrentScript = current_script_save;
					gEidosExecutingRuntimeScript = executing_runtime_script_save;
				}
				
				throw;
			}
			
			// Restore the normal error context in the event that no exception occurring within the lambda
			gEidosCharacterStartOfError = error_start_save;
			gEidosCharacterEndOfError = error_end_save;
			gEidosCharacterStartOfErrorUTF16 = error_start_save_UTF16;
			gEidosCharacterEndOfErrorUTF16 = error_end_save_UTF16;
			gEidosCurrentScript = current_script_save;
			gEidosExecutingRuntimeScript = executing_runtime_script_save;
			
			return sel_coeff;
		}
	}
	EIDOS_TERMINATION << "ERROR (MutationType::DrawSelectionCoefficient): (internal error) unexpected dfe_type_ value." << EidosTerminate();
}

// This is unused except by debugging code and in the debugger itself
std::ostream &operator<<(std::ostream &p_outstream, const MutationType &p_mutation_type)
{
	p_outstream << "MutationType{dominance_coeff_ " << p_mutation_type.dominance_coeff_ << ", dfe_type_ '" << p_mutation_type.dfe_type_ << "', dfe_parameters_ <";
	
	if (p_mutation_type.dfe_parameters_.size() > 0)
	{
		for (unsigned int i = 0; i < p_mutation_type.dfe_parameters_.size(); ++i)
		{
			p_outstream << p_mutation_type.dfe_parameters_[i];
			
			if (i < p_mutation_type.dfe_parameters_.size() - 1)
				p_outstream << " ";
		}
	}
	else
	{
		for (unsigned int i = 0; i < p_mutation_type.dfe_strings_.size(); ++i)
		{
			p_outstream << "\"" << p_mutation_type.dfe_strings_[i] << "\"";
			
			if (i < p_mutation_type.dfe_strings_.size() - 1)
				p_outstream << " ";
		}
	}
	
	p_outstream << ">}";
	
	return p_outstream;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *MutationType::Class(void) const
{
	return gSLiM_MutationType_Class;
}

void MutationType::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<m" << mutation_type_id_ << ">";
}

EidosValue_SP MutationType::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:						// ACCELERATED
		{
			if (!cached_value_muttype_id_)
				cached_value_muttype_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(mutation_type_id_));
			return cached_value_muttype_id_;
		}
		case gID_distributionType:
		{
			static EidosValue_SP static_dfe_string_f;
			static EidosValue_SP static_dfe_string_g;
			static EidosValue_SP static_dfe_string_e;
			static EidosValue_SP static_dfe_string_n;
			static EidosValue_SP static_dfe_string_w;
			static EidosValue_SP static_dfe_string_s;
			
			if (!static_dfe_string_f)
			{
				static_dfe_string_f = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_f));
				static_dfe_string_g = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_g));
				static_dfe_string_e = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_e));
				static_dfe_string_n = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_n));
				static_dfe_string_w = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_w));
				static_dfe_string_s = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_s));
			}
			
			switch (dfe_type_)
			{
				case DFEType::kFixed:			return static_dfe_string_f;
				case DFEType::kGamma:			return static_dfe_string_g;
				case DFEType::kExponential:		return static_dfe_string_e;
				case DFEType::kNormal:			return static_dfe_string_n;
				case DFEType::kWeibull:			return static_dfe_string_w;
				case DFEType::kScript:			return static_dfe_string_s;
				default:						return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_distributionParams:
		{
			if (dfe_parameters_.size() > 0)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(dfe_parameters_));
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector(dfe_strings_));
		}
			
			// variables
		case gEidosID_color:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(color_));
		case gID_colorSubstitution:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(color_sub_));
		case gID_convertToSubstitution:
			return (convert_to_substitution_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_dominanceCoeff:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(dominance_coeff_));
		case gID_mutationStackGroup:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(stack_group_));
		case gID_nucleotideBased:
			return (nucleotide_based_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutationStackPolicy:
		{
			static EidosValue_SP static_policy_string_s;
			static EidosValue_SP static_policy_string_f;
			static EidosValue_SP static_policy_string_l;
			
			if (!static_policy_string_s)
			{
				static_policy_string_s = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gEidosStr_s));
				static_policy_string_f = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_f));
				static_policy_string_l = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_l));
			}
			
			switch (stack_policy_)
			{
				case MutationStackPolicy::kStack:		return static_policy_string_s;
				case MutationStackPolicy::kKeepFirst:	return static_policy_string_f;
				case MutationStackPolicy::kKeepLast:	return static_policy_string_l;
				default:								return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_tag:						// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (MutationType::GetProperty): property tag accessed on mutation type before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

EidosValue *MutationType::GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		MutationType *value = (MutationType *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->mutation_type_id_, value_index);
	}
	
	return int_result;
}

EidosValue *MutationType::GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		MutationType *value = (MutationType *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (MutationType::GetProperty): property tag accessed on mutation type before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *MutationType::GetProperty_Accelerated_dominanceCoeff(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		MutationType *value = (MutationType *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->dominance_coeff_, value_index);
	}
	
	return float_result;
}

void MutationType::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gEidosID_color:
		{
			color_ = p_value.StringAtIndex(0, nullptr);
			if (!color_.empty())
				Eidos_GetColorComponents(color_, &color_red_, &color_green_, &color_blue_);
			return;
		}
			
		case gID_colorSubstitution:
		{
			color_sub_ = p_value.StringAtIndex(0, nullptr);
			if (!color_sub_.empty())
				Eidos_GetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
			return;
		}
			
		case gID_convertToSubstitution:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			convert_to_substitution_ = value;
			return;
		}
			
		case gID_dominanceCoeff:
		{
			double value = p_value.FloatAtIndex(0, nullptr);
			
			dominance_coeff_ = static_cast<slim_selcoeff_t>(value);		// intentionally no bounds check
			
			// Changing the dominance coefficient means that the cached fitness effects of all mutations using this type
			// become invalid.  We set a flag here to indicate that values that depend on us need to be recached.
			dominance_coeff_changed_ = true;
			sim_.any_dominance_coeff_changed_ = true;
			sim_.mutation_types_changed_ = true;
			
			return;
		}
			
		case gID_mutationStackGroup:
		{
			int64_t new_group = p_value.IntAtIndex(0, nullptr);
			
			if (nucleotide_based_ && (new_group != -1))
				EIDOS_TERMINATION << "ERROR (MutationType::SetProperty): property " << Eidos_StringForGlobalStringID(p_property_id) << " must be -1 for nucleotide-based mutation types." << EidosTerminate();
			
			stack_group_ = new_group;
			
			sim_.MutationStackPolicyChanged();
			return;
		}
			
		case gID_mutationStackPolicy:
		{
			std::string value = p_value.StringAtIndex(0, nullptr);
			
			if (nucleotide_based_ && (value != gStr_l))
				EIDOS_TERMINATION << "ERROR (MutationType::SetProperty): property " << Eidos_StringForGlobalStringID(p_property_id) << " must be \"l\" for nucleotide-based mutation types." << EidosTerminate();
			
			if (value.compare(gEidosStr_s) == 0)
				stack_policy_ = MutationStackPolicy::kStack;
			else if (value.compare(gStr_f) == 0)
				stack_policy_ = MutationStackPolicy::kKeepFirst;
			else if (value.compare(gStr_l) == 0)
				stack_policy_ = MutationStackPolicy::kKeepLast;
			else
				EIDOS_TERMINATION << "ERROR (MutationType::SetProperty): new value for property " << Eidos_StringForGlobalStringID(p_property_id) << " must be \"s\", \"f\", or \"l\"." << EidosTerminate();
			
			sim_.MutationStackPolicyChanged();
			return;
		}
			
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

void MutationType::SetProperty_Accelerated_convertToSubstitution(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = p_source.LogicalAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->convert_to_substitution_ = source_value;
	}
	else
	{
		const eidos_logical_t *source_data = p_source.LogicalVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->convert_to_substitution_ = source_data[value_index];
	}
}

void MutationType::SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

EidosValue_SP MutationType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_drawSelectionCoefficient:	return ExecuteMethod_drawSelectionCoefficient(p_method_id, p_arguments, p_interpreter);
		case gID_setDistribution:			return ExecuteMethod_setDistribution(p_method_id, p_arguments, p_interpreter);
		default:							return EidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (float)drawSelectionCoefficient([integer$ n = 1])
//
EidosValue_SP MutationType::ExecuteMethod_drawSelectionCoefficient(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_SP result_SP(nullptr);
	EidosValue *n_value = p_arguments[0].get();
	int64_t num_draws = n_value->IntAtIndex(0, nullptr);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (ExecuteMethod_drawSelectionCoefficient): drawSelectionCoefficient() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	
	if (num_draws == 1)
	{
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(DrawSelectionCoefficient()));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(num_draws);
		result_SP = EidosValue_SP(float_result);
		
		for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			float_result->set_float_no_check(DrawSelectionCoefficient(), draw_index);
	}
	
	return result_SP;
}

//	*********************	- (void)setDistribution(string$ distributionType, ...)
//
EidosValue_SP MutationType::ExecuteMethod_setDistribution(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *distributionType_value = p_arguments[0].get();
	std::string dfe_type_string = distributionType_value->StringAtIndex(0, nullptr);
	
	// Parse the DFE type and parameters, and do various sanity checks
	DFEType dfe_type;
	std::vector<double> dfe_parameters;
	std::vector<std::string> dfe_strings;
	
	MutationType::ParseDFEParameters(dfe_type_string, p_arguments.data() + 1, (int)p_arguments.size() - 1, &dfe_type, &dfe_parameters, &dfe_strings);
	
	// Everything seems to be in order, so replace our distribution info with the new info
	dfe_type_ = dfe_type;
	dfe_parameters_ = dfe_parameters;
	dfe_strings_ = dfe_strings;
	
	// mark that mutation types changed, so they get redisplayed in SLiMgui
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	
	sim.mutation_types_changed_ = true;
	
	// check whether we are now using a DFE type that is non-neutral; check and set pure_neutral_ and all_pure_neutral_DFE_
	if ((dfe_type_ != DFEType::kFixed) || (dfe_parameters_[0] != 0.0))
	{
		sim.pure_neutral_ = false;
		all_pure_neutral_DFE_ = false;
	}
	
	return gStaticEidosValueVOID;
}


//
//	MutationType_Class
//
#pragma mark -
#pragma mark MutationType_Class
#pragma mark -

class MutationType_Class : public EidosDictionary_Class
{
public:
	MutationType_Class(const MutationType_Class &p_original) = delete;	// no copy-construct
	MutationType_Class& operator=(const MutationType_Class&) = delete;	// no copying
	inline MutationType_Class(void) { }
	
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};

EidosObjectClass *gSLiM_MutationType_Class = new MutationType_Class();


const std::string &MutationType_Class::ElementType(void) const
{
	return gStr_MutationType;
}

const std::vector<EidosPropertySignature_CSP> *MutationType_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*EidosDictionary_Class::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(MutationType::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_convertToSubstitution,	false,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedSet(MutationType::SetProperty_Accelerated_convertToSubstitution));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_distributionType,		true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_distributionParams,		true,	kEidosValueMaskFloat | kEidosValueMaskString)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_dominanceCoeff,			false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(MutationType::GetProperty_Accelerated_dominanceCoeff));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationStackGroup,		false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationStackPolicy,	false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideBased,		true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(MutationType::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(MutationType::SetProperty_Accelerated_tag));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_color,				false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_colorSubstitution,		false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *MutationType_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*EidosDictionary_Class::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawSelectionCoefficient, kEidosValueMaskFloat))->AddInt_OS("n", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setDistribution, kEidosValueMaskVOID))->AddString_S("distributionType")->AddEllipsis());
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}
































































