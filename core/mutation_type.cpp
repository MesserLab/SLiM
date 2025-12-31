//
//  mutation_type.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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
#include "species.h"
#include "community.h"
#include "mutation_block.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <utility>


// stream output for enumerations
std::ostream& operator<<(std::ostream& p_out, DESType p_DES_type)
{
	switch (p_DES_type)
	{
		case DESType::kFixed:			p_out << gStr_f;		break;
		case DESType::kGamma:			p_out << gStr_g;		break;
		case DESType::kExponential:		p_out << gStr_e;		break;
		case DESType::kNormal:			p_out << gEidosStr_n;	break;
		case DESType::kWeibull:			p_out << gStr_w;		break;
		case DESType::kLaplace:			p_out << gStr_p;		break;
		case DESType::kScript:			p_out << gEidosStr_s;	break;
	}
	
	return p_out;
}


#pragma mark -
#pragma mark MutationType
#pragma mark -

#ifdef SLIMGUI
MutationType::MutationType(Species &p_species, slim_objectid_t p_mutation_type_id, double p_dominance_coeff, bool p_nuc_based, DESType p_DES_type, std::vector<double> p_DES_parameters, std::vector<std::string> p_DES_strings, int p_mutation_type_index) :
#else
MutationType::MutationType(Species &p_species, slim_objectid_t p_mutation_type_id, double p_dominance_coeff, bool p_nuc_based, DESType p_DES_type, std::vector<double> p_DES_parameters, std::vector<std::string> p_DES_strings) :
#endif
self_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('m', p_mutation_type_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_MutationType_Class))),
	species_(p_species), mutation_type_id_(p_mutation_type_id), nucleotide_based_(p_nuc_based), convert_to_substitution_(false), stack_policy_(MutationStackPolicy::kStack), stack_group_(p_mutation_type_id), cached_DES_script_(nullptr)
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	, muttype_registry_call_count_(0), keeping_muttype_registry_(false)
#endif
#ifdef SLIMGUI
	, mutation_type_index_(p_mutation_type_index)
#endif
{
	// self_symbol_ is always a constant, but can't be marked as such on construction
	self_symbol_.second->MarkAsConstant();
	
	// In WF models, convertToSubstitution defaults to T; in nonWF models it defaults to F as specified above
	if (species_.community_.ModelType() == SLiMModelType::kModelTypeWF)
		convert_to_substitution_ = true;
	
	if ((p_DES_parameters.size() == 0) && (p_DES_strings.size() == 0))
		EIDOS_TERMINATION << "ERROR (MutationType::MutationType): invalid mutation type parameters." << EidosTerminate();
	// intentionally no bounds checks for DES parameters; the count of DES parameters is checked prior to construction
	// intentionally no bounds check for dominance_coeff_
	
	// determine whether this mutation type has a neutral DES
	// note that we do not set Species.pure_neutral_ here; we wait until this muttype is used
	all_neutral_DES_ = ((p_DES_type == DESType::kFixed) && (p_DES_parameters[0] == 0.0));
	
	// initially, whether a mutation type has any neutral mutations is inherited from whether it has a neutral DES
	// note that this flag will be cleared if any mutation of this type has its effect changed to non-neutral
	all_neutral_mutations_ = all_neutral_DES_;
	
	// set up DE entries for all traits; every trait is initialized identically, from the parameters given
	EffectDistributionInfo DES_info;
	
	DES_info.default_dominance_coeff_ = static_cast<slim_effect_t>(p_dominance_coeff);	// note this can be NAN now, representing independent dominance
	DES_info.default_hemizygous_dominance_coeff_ = 1.0;
	DES_info.DES_type_ = p_DES_type;
	DES_info.DES_parameters_ = p_DES_parameters;
	DES_info.DES_strings_ = p_DES_strings;
	
	for (int trait_index = 0; trait_index < species_.TraitCount(); trait_index++)
		effect_distributions_.push_back(DES_info);
	
	// Nucleotide-based mutations use a special stacking group, -1, and always use stacking policy "l"
	if (p_nuc_based)
	{
		stack_policy_ = MutationStackPolicy::kKeepLast;
		stack_group_ = -1;
	}
	
	// The fact that we have been created means that stacking policy has changed and needs to be checked
	species_.MutationStackPolicyChanged();
}

MutationType::~MutationType(void)
{
	delete cached_DES_script_;
	cached_DES_script_ = nullptr;
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	if (keeping_muttype_registry_)
	{
		muttype_registry_.clear();
		keeping_muttype_registry_ = false;
	}
#endif
}

void MutationType::ParseDESParameters(std::string &p_DES_type_string, const EidosValue_SP *const p_arguments, int p_argument_count, DESType *p_DES_type, std::vector<double> *p_DES_parameters, std::vector<std::string> *p_DES_strings)
{
	// First we figure out the DES type from p_DES_type_string, and set up expectations based on that
	int expected_DES_param_count = 0;
	bool params_are_numeric = true;
	
	if (p_DES_type_string.compare(gStr_f) == 0)
	{
		*p_DES_type = DESType::kFixed;
		expected_DES_param_count = 1;
	}
	else if (p_DES_type_string.compare(gStr_g) == 0)
	{
		*p_DES_type = DESType::kGamma;
		expected_DES_param_count = 2;
	}
	else if (p_DES_type_string.compare(gStr_e) == 0)
	{
		*p_DES_type = DESType::kExponential;
		expected_DES_param_count = 1;
	}
	else if (p_DES_type_string.compare(gEidosStr_n) == 0)
	{
		*p_DES_type = DESType::kNormal;
		expected_DES_param_count = 2;
	}
	else if (p_DES_type_string.compare(gStr_w) == 0)
	{
		*p_DES_type = DESType::kWeibull;
		expected_DES_param_count = 2;
	}
	else if (p_DES_type_string.compare(gStr_p) == 0)
	{
		*p_DES_type = DESType::kLaplace;
		expected_DES_param_count = 2;
	}
	else if (p_DES_type_string.compare(gEidosStr_s) == 0)
	{
		*p_DES_type = DESType::kScript;
		expected_DES_param_count = 1;
		params_are_numeric = false;
	}
	else
		EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): distribution type '" << p_DES_type_string << "' must be 'f', 'g', 'e', 'n', 'w', or 's'." << EidosTerminate();
	
	if (p_argument_count != expected_DES_param_count)
		EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): distribution type '" << *p_DES_type << "' requires exactly " << expected_DES_param_count << " DES parameter" << (expected_DES_param_count == 1 ? "" : "s") << "." << EidosTerminate();
	
	// Next we extract the parameter values, checking their types in accordance with params_are_numeric
	p_DES_parameters->clear();
	p_DES_strings->clear();
	
	for (int DES_param_index = 0; DES_param_index < expected_DES_param_count; ++DES_param_index)
	{
		EidosValue *DES_param_value = p_arguments[DES_param_index].get();
		EidosValueType DES_param_type = DES_param_value->Type();
		
		if (params_are_numeric)
		{
			if ((DES_param_type != EidosValueType::kValueFloat) && (DES_param_type != EidosValueType::kValueInt))
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): the parameters for a DES of type '" << *p_DES_type << "' must be of type numeric (integer or float)." << EidosTerminate();
			
			p_DES_parameters->emplace_back(DES_param_value->NumericAtIndex_NOCAST(0, nullptr));
		}
		else
		{
			if (DES_param_type != EidosValueType::kValueString)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): the parameters for a DES of type '" << *p_DES_type << "' must be of type string." << EidosTerminate();
			
			p_DES_strings->emplace_back(DES_param_value->StringAtIndex_NOCAST(0, nullptr));
		}
	}
	
	// Finally, we bounds-check the DES parameters in the cases where there is a hard bound
	switch (*p_DES_type)
	{
		case DESType::kFixed:
			// no limits on fixed DESs; we could check that s >= -1, but that assumes that the selection coefficients are being used as selection coefficients
			break;
		case DESType::kGamma:
			// mean is unrestricted, shape parameter must be >0 (officially mean > 0, but we allow mean <= 0 and the GSL handles it)
			if ((*p_DES_parameters)[1] <= 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): a DES of type 'g' must have a shape parameter > 0." << EidosTerminate();
			break;
		case DESType::kExponential:
			// no limits on exponential DESs (officially scale > 0, but we allow scale <= 0 and the GSL handles it)
			break;
		case DESType::kNormal:
			// mean is unrestricted, sd parameter must be >= 0
			if ((*p_DES_parameters)[1] < 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): a DES of type 'n' must have a standard deviation parameter >= 0." << EidosTerminate();
			break;
		case DESType::kWeibull:
			// scale and shape must both be > 0
			if ((*p_DES_parameters)[0] <= 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): a DES of type 'w' must have a scale parameter > 0." << EidosTerminate();
			if ((*p_DES_parameters)[1] <= 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): a DES of type 'w' must have a shape parameter > 0." << EidosTerminate();
			break;
		case DESType::kLaplace:
			// mean is unrestricted, scale parameter must be > 0
			if ((*p_DES_parameters)[1] <= 0.0)
				EIDOS_TERMINATION << "ERROR (MutationType::ParseDESParameters): a DES of type 'p' must have a scale parameter > 0." << EidosTerminate();
			break;
		case DESType::kScript:
			// no limits on script here; the script is checked when it gets tokenized/parsed/executed
			break;
	}
}

void MutationType::SelfConsistencyCheck(const std::string &p_message_end)
{
	// note that we don't check for mutation_block_ being nullptr here because we get called before that happens,
	// unlike SelfConsistencyCheck() for Mutation and Substitution, where the mutation block necessarily exists
	const std::vector<Trait *> &traits = species_.Traits();
	
	if (effect_distributions_.size() != traits.size())
		EIDOS_TERMINATION << "ERROR (MutationType::SelfConsistencyCheck): (internal error) effect_distributions_ size does not match traits.size()" << p_message_end << "." << EidosTerminate();
	
	if (effect_distributions_.size() > 0)
	{
		bool is_independent_dominance = std::isnan(effect_distributions_[0].default_dominance_coeff_);
		
		for (EffectDistributionInfo &des_info : effect_distributions_)
		{
			if (std::isnan(des_info.default_dominance_coeff_) != is_independent_dominance)
				EIDOS_TERMINATION << "ERROR (MutationType::SelfConsistencyCheck): mutation type independent dominance state is inconsistent" << p_message_end << "." << EidosTerminate();
			
			if (std::isinf(des_info.default_dominance_coeff_))	// NAN allowed
				EIDOS_TERMINATION << "ERROR (MutationType::SelfConsistencyCheck): mutation type default dominance is infinite" << p_message_end << "." << EidosTerminate();
			
			if (!std::isfinite(des_info.default_hemizygous_dominance_coeff_))
				EIDOS_TERMINATION << "ERROR (MutationType::SelfConsistencyCheck): mutation type default hemizygous dominance is non-finite" << p_message_end << "." << EidosTerminate();
		}
	}
}

slim_effect_t MutationType::DrawEffectForTrait(int64_t p_trait_index) const
{
	const EffectDistributionInfo &DES_info = effect_distributions_[p_trait_index];
	
	// BCH 11/11/2022: Note that EIDOS_GSL_RNG(omp_get_thread_num()) can take a little bit of time when running
	// parallel.  We don't want to pass the RNG in, though, because that would slow down the single-threaded
	// case, where the EIDOS_GSL_RNG(omp_get_thread_num()) call basically compiles away to a global var access.
	// So here and in similar places, we fetch the RNG rather than passing it in to keep single-threaded fast.
	switch (DES_info.DES_type_)
	{
		case DESType::kFixed:			return static_cast<slim_effect_t>(DES_info.DES_parameters_[0]);
			
		case DESType::kGamma:
		{
			gsl_rng *rng_gsl = EIDOS_GSL_RNG(omp_get_thread_num());
			return static_cast<slim_effect_t>(gsl_ran_gamma(rng_gsl, DES_info.DES_parameters_[1], DES_info.DES_parameters_[0] / DES_info.DES_parameters_[1]));
		}
			
		case DESType::kExponential:
		{
			gsl_rng *rng_gsl = EIDOS_GSL_RNG(omp_get_thread_num());
			return static_cast<slim_effect_t>(gsl_ran_exponential(rng_gsl, DES_info.DES_parameters_[0]));
		}
			
		case DESType::kNormal:
		{
			gsl_rng *rng_gsl = EIDOS_GSL_RNG(omp_get_thread_num());
			return static_cast<slim_effect_t>(gsl_ran_gaussian(rng_gsl, DES_info.DES_parameters_[1]) + DES_info.DES_parameters_[0]);
		}
			
		case DESType::kWeibull:
		{
			gsl_rng *rng_gsl = EIDOS_GSL_RNG(omp_get_thread_num());
			return static_cast<slim_effect_t>(gsl_ran_weibull(rng_gsl, DES_info.DES_parameters_[0], DES_info.DES_parameters_[1]));
		}
			
		case DESType::kLaplace:
		{
			gsl_rng *rng_gsl = EIDOS_GSL_RNG(omp_get_thread_num());
			return static_cast<slim_effect_t>(gsl_ran_laplace(rng_gsl, DES_info.DES_parameters_[1]) + DES_info.DES_parameters_[0]);
		}
			
		case DESType::kScript:
		{
			// We have a script string that we need to execute, and it will return a float or integer to us.  This
			// is basically a lambda call, so the code here is parallel to the executeLambda() code in many ways.
			
#ifdef DEBUG_LOCKS_ENABLED
			// When running multi-threaded, this code is not re-entrant because it runs an Eidos interpreter.  We use
			// EidosDebugLock to enforce that.  In addition, it can raise, so the caller must be prepared for that.
			static EidosDebugLock DrawEffectForTrait_InterpreterLock("DrawEffectForTrait_InterpreterLock");
			
			DrawEffectForTrait_InterpreterLock.start_critical(0);
#endif
			
			double sel_coeff;
			
			// Errors in lambdas should be reported for the lambda script, not for the calling script,
			// if possible.  In the GUI this does not work well, however; there, errors should be
			// reported as occurring in the call to executeLambda().  Here we save off the current
			// error context and set up the error context for reporting errors inside the lambda,
			// in case that is possible; see how exceptions are handled below.
			EidosErrorContext error_context_save = gEidosErrorContext;
			
			// We try to do tokenization and parsing once per script, by caching the script
			if (!cached_DES_script_)
			{
				std::string script_string = DES_info.DES_strings_[0];
				cached_DES_script_ = new EidosScript(script_string);
				
				gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, cached_DES_script_};
				
				try
				{
					cached_DES_script_->Tokenize();
					cached_DES_script_->ParseInterpreterBlockToAST(false);
				}
				catch (...)
				{
					if (gEidosTerminateThrows)
					{
						gEidosErrorContext = error_context_save;
						TranslateErrorContextToUserScript("DrawEffectForTrait()");
					}
					
					delete cached_DES_script_;
					cached_DES_script_ = nullptr;
					
#ifdef DEBUG_LOCKS_ENABLED
					DrawEffectForTrait_InterpreterLock.end_critical();
#endif
					
					EIDOS_TERMINATION << "ERROR (MutationType::DrawEffectForTrait): tokenize/parse error in type 's' DES callback script." << EidosTerminate(nullptr);
				}
			}
			
			// Execute inside try/catch so we can handle errors well
			gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, cached_DES_script_};
			
			try
			{
				Community &community = species_.community_;
				EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &community.SymbolTable());
				EidosFunctionMap &function_map = community.FunctionMap();
				EidosInterpreter interpreter(*cached_DES_script_, client_symbols, function_map, &community, SLIM_OUTSTREAM, SLIM_ERRSTREAM
#ifdef SLIMGUI
					, community.check_infinite_loops_
#endif
					);
				
				EidosValue_SP result_SP = interpreter.EvaluateInterpreterBlock(false, true);	// do not print output, return the last statement value
				EidosValue *result = result_SP.get();
				EidosValueType result_type = result->Type();
				int result_count = result->Count();
				
				if ((result_type == EidosValueType::kValueFloat) && (result_count == 1))
					sel_coeff = result->FloatData()[0];
				else if ((result_type == EidosValueType::kValueInt) && (result_count == 1))
					sel_coeff = result->IntData()[0];
				else
					EIDOS_TERMINATION << "ERROR (MutationType::DrawEffectForTrait): type 's' DES callbacks must provide a singleton float or integer return value." << EidosTerminate(nullptr);
			}
			catch (...)
			{
				// If exceptions throw, then we want to set up the error information to highlight the
				// executeLambda() that failed, since we can't highlight the actual error.  (If exceptions
				// don't throw, this catch block will never be hit; exit() will already have been called
				// and the error will have been reported from the context of the lambda script string.)
				if (gEidosTerminateThrows)
				{
					// In some cases, such as if the error occurred in a derived user-defined function, we can
					// actually get a user script error context at this point, and don't need to intervene.
					if (!gEidosErrorContext.currentScript || (gEidosErrorContext.currentScript->UserScriptUTF16Offset() == -1))
					{
						gEidosErrorContext = error_context_save;
						TranslateErrorContextToUserScript("DrawEffectForTrait()");
					}
				}
				
#ifdef DEBUG_LOCKS_ENABLED
				DrawEffectForTrait_InterpreterLock.end_critical();
#endif
				
				throw;
			}
			
			// Restore the normal error context in the event that no exception occurring within the lambda
			gEidosErrorContext = error_context_save;
			
#ifdef DEBUG_LOCKS_ENABLED
			DrawEffectForTrait_InterpreterLock.end_critical();
#endif
			
			return static_cast<slim_effect_t>(sel_coeff);
		}
	}
	EIDOS_TERMINATION << "ERROR (MutationType::DrawEffectForTrait): (internal error) unexpected DES_type_ value." << EidosTerminate();
}

// This is unused except by debugging code and in the debugger itself
// FIXME MULTITRAIT commented this out for now
/*std::ostream &operator<<(std::ostream &p_outstream, const MutationType &p_mutation_type)
{
	p_outstream << "MutationType{default_dominance_coeff_ " << p_mutation_type.default_dominance_coeff_ << ", DES_type_ '" << p_mutation_type.des_type_ << "', DES_parameters_ <";
	
	if (p_mutation_type.des_parameters_.size() > 0)
	{
		for (unsigned int i = 0; i < p_mutation_type.des_parameters_.size(); ++i)
		{
			p_outstream << p_mutation_type.des_parameters_[i];
			
			if (i < p_mutation_type.des_parameters_.size() - 1)
				p_outstream << " ";
		}
	}
	else
	{
		for (unsigned int i = 0; i < p_mutation_type.des_strings_.size(); ++i)
		{
			p_outstream << "\"" << p_mutation_type.des_strings_[i] << "\"";
			
			if (i < p_mutation_type.des_strings_.size() - 1)
				p_outstream << " ";
		}
	}
	
	p_outstream << ">}";
	
	return p_outstream;
}*/


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *MutationType::Class(void) const
{
	return gSLiM_MutationType_Class;
}

void MutationType::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<m" << mutation_type_id_ << ">";
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
				cached_value_muttype_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_type_id_));
			return cached_value_muttype_id_;
		}
		case gID_species:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(&species_, gSLiM_Species_Class));
		}
			
			// variables
		case gEidosID_color:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(color_));
		case gID_colorSubstitution:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(color_sub_));
		case gID_convertToSubstitution:
			return (convert_to_substitution_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutationStackGroup:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(stack_group_));
		case gID_nucleotideBased:
			return (nucleotide_based_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutationStackPolicy:
		{
			static EidosValue_SP static_policy_string_s;
			static EidosValue_SP static_policy_string_f;
			static EidosValue_SP static_policy_string_l;
			
#pragma omp critical (GetProperty_mutationStackPolicy_cache)
			{
				if (!static_policy_string_s)
				{
					THREAD_SAFETY_IN_ACTIVE_PARALLEL("MutationType::GetProperty(): usage of statics");
					
					static_policy_string_s = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gEidosStr_s));
					static_policy_string_f = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gStr_f));
					static_policy_string_l = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(gStr_l));
				}
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
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *MutationType::GetProperty_Accelerated_id(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		MutationType *value = (MutationType *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->mutation_type_id_, value_index);
	}
	
	return int_result;
}

EidosValue *MutationType::GetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		MutationType *value = (MutationType *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (MutationType::GetProperty_Accelerated_tag): property tag accessed on mutation type before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

void MutationType::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gEidosID_color:
		{
			color_ = p_value.StringAtIndex_NOCAST(0, nullptr);
			if (!color_.empty())
				Eidos_GetColorComponents(color_, &color_red_, &color_green_, &color_blue_);
			return;
		}
			
		case gID_colorSubstitution:
		{
			color_sub_ = p_value.StringAtIndex_NOCAST(0, nullptr);
			if (!color_sub_.empty())
				Eidos_GetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
			return;
		}
			
		case gID_convertToSubstitution:				// ACCELERATED
		{
			eidos_logical_t value = p_value.LogicalAtIndex_NOCAST(0, nullptr);
			
			convert_to_substitution_ = value;
			return;
		}
			
		case gID_mutationStackGroup:
		{
			int64_t new_group = p_value.IntAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_based_ && (new_group != -1))
				EIDOS_TERMINATION << "ERROR (MutationType::SetProperty): property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " must be -1 for nucleotide-based mutation types." << EidosTerminate();
			
			stack_group_ = new_group;
			
			species_.MutationStackPolicyChanged();
			return;
		}
			
		case gID_mutationStackPolicy:
		{
			std::string value = p_value.StringAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_based_ && (value != gStr_l))
				EIDOS_TERMINATION << "ERROR (MutationType::SetProperty): property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " must be 'l' for nucleotide-based mutation types." << EidosTerminate();
			
			if (value.compare(gEidosStr_s) == 0)
				stack_policy_ = MutationStackPolicy::kStack;
			else if (value.compare(gStr_f) == 0)
				stack_policy_ = MutationStackPolicy::kKeepFirst;
			else if (value.compare(gStr_l) == 0)
				stack_policy_ = MutationStackPolicy::kKeepLast;
			else
				EIDOS_TERMINATION << "ERROR (MutationType::SetProperty): new value for property " << EidosStringRegistry::StringForGlobalStringID(p_property_id) << " must be 's', 'f', or 'l'." << EidosTerminate();
			
			species_.MutationStackPolicyChanged();
			return;
		}
			
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

void MutationType::SetProperty_Accelerated_convertToSubstitution(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
#pragma unused (p_property_id)
	if (p_source_size == 1)
	{
		eidos_logical_t source_value = p_source.LogicalAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->convert_to_substitution_ = source_value;
	}
	else
	{
		const eidos_logical_t *source_data = p_source.LogicalData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->convert_to_substitution_ = source_data[value_index];
	}
}

void MutationType::SetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
#pragma unused (p_property_id)
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((MutationType *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

EidosValue_SP MutationType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_defaultDominanceForTrait:				return ExecuteMethod_defaultDominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_defaultHemizygousDominanceForTrait:	return ExecuteMethod_defaultHemizygousDominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_effectDistributionTypeForTrait:		return ExecuteMethod_effectDistributionTypeForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_effectDistributionParamsForTrait:		return ExecuteMethod_effectDistributionParamsForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_drawEffectForTrait:					return ExecuteMethod_drawEffectForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_setDefaultDominanceForTrait:			return ExecuteMethod_setDefaultDominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_setDefaultHemizygousDominanceForTrait:	return ExecuteMethod_setDefaultHemizygousDominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_setEffectDistributionForTrait:			return ExecuteMethod_setEffectDistributionForTrait(p_method_id, p_arguments, p_interpreter);
		default:										return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (float$)defaultDominanceForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP MutationType::ExecuteMethod_defaultDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "defaultDominanceForTrait");
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)DefaultDominanceForTrait(trait_index)));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
			float_result->push_float_no_check((double)DefaultDominanceForTrait(trait_index));
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (float$)defaultHemizygousDominanceForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP MutationType::ExecuteMethod_defaultHemizygousDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "defaultHemizygousDominanceForTrait");
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)DefaultHemizygousDominanceForTrait(trait_index)));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
			float_result->push_float_no_check((double)DefaultHemizygousDominanceForTrait(trait_index));
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (fs)effectDistributionParamsForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP MutationType::ExecuteMethod_effectDistributionParamsForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "effectDistributionParamsForTrait");
	
	// decide whether doing floats or strings; must be the same for all
	bool is_float = false;
	bool is_string = false;
	
	for (int64_t trait_index : trait_indices)
	{
		EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
		
		if (DES_info.DES_parameters_.size() > 0)
			is_float = true;
		else
			is_string = true;
	}
	
	if (is_float && is_string)
		EIDOS_TERMINATION << "ERROR (ExecuteMethod_effectDistributionParamsForTrait): effectDistributionParamsForTrait() requires all specified traits to have either float or string parameters (not a mixture) for their distributions of effects." << EidosTerminate(nullptr);
	
	if (is_float)
	{
		EidosValue_Float *float_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Float();
		
		for (int64_t trait_index : trait_indices)
		{
			EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
			
			for (double param : DES_info.DES_parameters_)
				float_result->push_float(param);
		}
		
		return EidosValue_SP(float_result);
	}
	else
	{
		EidosValue_String *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String();
		
		for (int64_t trait_index : trait_indices)
		{
			EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
			
			for (const std::string &param : DES_info.DES_strings_)
				string_result->PushString(param);
		}
		
		return EidosValue_SP(string_result);
	}
}

//	*********************	- (string$)effectDistributionTypeForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP MutationType::ExecuteMethod_effectDistributionTypeForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "effectDistributionTypeForTrait");
	
	// assemble the result
	EidosValue_String *string_result = new (gEidosValuePool->AllocateChunk()) EidosValue_String();
	
	for (int64_t trait_index : trait_indices)
	{
		EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
		
		switch (DES_info.DES_type_)
		{
			case DESType::kFixed:			string_result->PushString(gStr_f); break;
			case DESType::kGamma:			string_result->PushString(gStr_g); break;
			case DESType::kExponential:		string_result->PushString(gStr_e); break;
			case DESType::kNormal:			string_result->PushString(gEidosStr_n); break;
			case DESType::kWeibull:			string_result->PushString(gStr_w); break;
			case DESType::kLaplace:			string_result->PushString(gStr_p); break;
			case DESType::kScript:			string_result->PushString(gEidosStr_s); break;
			default:						return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
		}
	}
	
	return EidosValue_SP(string_result);
}

//	*********************	- (float)drawEffectForTrait([Niso<Trait> trait = NULL], [integer$ n = 1])
//
EidosValue_SP MutationType::ExecuteMethod_drawEffectForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_SP result_SP(nullptr);
	EidosValue *trait_value = p_arguments[0].get();
	EidosValue *n_value = p_arguments[1].get();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "drawEffectForTrait");
	
	// get the number of effects to draw
	int64_t num_draws = n_value->IntAtIndex_NOCAST(0, nullptr);
	
	if (num_draws < 0)
		EIDOS_TERMINATION << "ERROR (ExecuteMethod_drawEffectForTrait): drawEffectForTrait() requires n to be greater than or equal to 0 (" << num_draws << " supplied)." << EidosTerminate(nullptr);
	
	if ((trait_indices.size() == 1) && (num_draws == 1))
	{
		int64_t trait_index = trait_indices[0];
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)DrawEffectForTrait(trait_index)));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size() * num_draws);
		
		// draw_index is the outer loop, so that we get num_draws sets of (one draw per trait)
		for (int64_t draw_index = 0; draw_index < num_draws; ++draw_index)
			for (int64_t trait_index : trait_indices)
				float_result->push_float_no_check((double)DrawEffectForTrait(trait_index));
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (void)setDefaultDominanceForTrait(Niso<Trait> trait, float dominance)
//
EidosValue_SP MutationType::ExecuteMethod_setDefaultDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	EidosValue *dominance_value = p_arguments[1].get();
	int dominance_count = dominance_value->Count();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "setDefaultDominanceForTrait");
	
	if (dominance_count == 1)
	{
		// get the dominance coefficient
		double dominance = dominance_value->FloatAtIndex_NOCAST(0, nullptr);
		
		for (int64_t trait_index : trait_indices)
		{
			EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
			
			DES_info.default_dominance_coeff_ = static_cast<slim_effect_t>(dominance);		// intentionally no bounds check
		}
	}
	else if (dominance_count == (int)trait_indices.size())
	{
		for (int dominance_index = 0; dominance_index < dominance_count; dominance_index++)
		{
			int64_t trait_index = trait_indices[dominance_index];
			EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
			double dominance = dominance_value->FloatAtIndex_NOCAST(dominance_index, nullptr);
			
			DES_info.default_dominance_coeff_ = static_cast<slim_effect_t>(dominance);		// intentionally no bounds check
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (ExecuteMethod_setDefaultDominanceForTrait): setDefaultDominanceForTrait() requires parameter dominance to be of length 1, or equal in length to the number of specified traits." << EidosTerminate(nullptr);
	
	// BCH 7/2/2025: Changing the default dominance coefficient no longer means that the cached fitness
	// effects of all mutations using this type become invalid; it is now just the *default* coefficient,
	// and changing it does not change the state of mutations that have already derived from it.  We do
	// still want to let the community know that a mutation type has changed, though.
	species_.community_.mutation_types_changed_ = true;
	
	SelfConsistencyCheck(" in setDefaultDominanceForTrait()");
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)setDefaultHemizygousDominanceForTrait(Niso<Trait> trait, float dominance)
//
EidosValue_SP MutationType::ExecuteMethod_setDefaultHemizygousDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	EidosValue *dominance_value = p_arguments[1].get();
	int dominance_count = dominance_value->Count();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "setDefaultHemizygousDominanceForTrait");
	
	if (dominance_count == 1)
	{
		// get the dominance coefficient
		double dominance = dominance_value->FloatAtIndex_NOCAST(0, nullptr);
		
		for (int64_t trait_index : trait_indices)
		{
			EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
			
			DES_info.default_hemizygous_dominance_coeff_ = static_cast<slim_effect_t>(dominance);		// intentionally no bounds check
		}
	}
	else if (dominance_count == (int)trait_indices.size())
	{
		for (int dominance_index = 0; dominance_index < dominance_count; dominance_index++)
		{
			int64_t trait_index = trait_indices[dominance_index];
			EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
			double dominance = dominance_value->FloatAtIndex_NOCAST(dominance_index, nullptr);
			
			DES_info.default_hemizygous_dominance_coeff_ = static_cast<slim_effect_t>(dominance);		// intentionally no bounds check
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (ExecuteMethod_setDefaultHemizygousDominanceForTrait): setDefaultHemizygousDominanceForTrait() requires parameter dominance to be of length 1, or equal in length to the number of specified traits." << EidosTerminate(nullptr);
	
	// BCH 7/2/2025: Changing the default dominance coefficient no longer means that the cached fitness
	// effects of all mutations using this type become invalid; it is now just the *default* coefficient,
	// and changing it does not change the state of mutations that have already derived from it.  We do
	// still want to let the community know that a mutation type has changed, though.
	species_.community_.mutation_types_changed_ = true;
	
	SelfConsistencyCheck(" in setDefaultDominanceForTrait()");
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)setEffectDistributionForTrait(Niso<Trait> trait, string$ distributionType, ...)
//
EidosValue_SP MutationType::ExecuteMethod_setEffectDistributionForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	EidosValue *distributionType_value = p_arguments[1].get();
	std::string DES_type_string = distributionType_value->StringAtIndex_NOCAST(0, nullptr);
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species_.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "setEffectDistributionForTrait");
	
	// Parse the DES type and parameters, and do various sanity checks
	DESType DES_type;
	std::vector<double> DES_parameters;
	std::vector<std::string> DES_strings;
	
	MutationType::ParseDESParameters(DES_type_string, p_arguments.data() + 2, (int)p_arguments.size() - 2, &DES_type, &DES_parameters, &DES_strings);
	
	// keep track of whether we have ever seen a type 's' (scripted) DES; if so, we switch to a slower case when evolving
	if (DES_type == DESType::kScript)
		species_.type_s_DESs_present_ = true;
	
	// Everything seems to be in order, so replace our distribution info (in each specified trait) with the new info
	for (int64_t trait_index : trait_indices)
	{
		EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
		
		DES_info.DES_type_ = DES_type;
		DES_info.DES_parameters_ = DES_parameters;
		DES_info.DES_strings_ = DES_strings;
	}
	
	// mark that mutation types changed, so they get redisplayed in SLiMgui
	species_.community_.mutation_types_changed_ = true;
	
	// check whether our DES for all traits is now neutral; we can change from non-neutral back to neutral
	all_neutral_DES_ = true;
	
	for (int64_t trait_index = 0; trait_index < species_.TraitCount(); ++trait_index)
	{
		EffectDistributionInfo &DES_info = effect_distributions_[trait_index];
		
		if ((DES_info.DES_type_ != DESType::kFixed) || (DES_info.DES_parameters_[0] != 0.0))
			all_neutral_DES_ = false;
	}
	
	// if our DES is non-neutral, set pure_neutral_ and all_neutral_mutations_ to false;
	// these flags are sticky, so we don't try to set them back to true again
	if (!all_neutral_DES_)
	{
		species_.pure_neutral_ = false;
		all_neutral_mutations_ = false;
	}
	
	return gStaticEidosValueVOID;
}


//
//	MutationType_Class
//
#pragma mark -
#pragma mark MutationType_Class
#pragma mark -

MutationType_Class *gSLiM_MutationType_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *MutationType_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("MutationType_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(MutationType::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_convertToSubstitution,	false,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedSet(MutationType::SetProperty_Accelerated_convertToSubstitution));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationStackGroup,		false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationStackPolicy,	false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideBased,		true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_species,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Species_Class)));
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
		THREAD_SAFETY_IN_ANY_PARALLEL("MutationType_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_defaultDominanceForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_defaultHemizygousDominanceForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_effectDistributionParamsForTrait, kEidosValueMaskFloat | kEidosValueMaskString))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_effectDistributionTypeForTrait, kEidosValueMaskString))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawEffectForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL)->AddInt_OS("n", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setDefaultDominanceForTrait, kEidosValueMaskVOID))->AddIntStringObject_N("trait", gSLiM_Trait_Class)->AddFloat("dominance"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setDefaultHemizygousDominanceForTrait, kEidosValueMaskVOID))->AddIntStringObject_N("trait", gSLiM_Trait_Class)->AddFloat("dominance"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setEffectDistributionForTrait, kEidosValueMaskVOID))->AddIntStringObject_N("trait", gSLiM_Trait_Class)->AddString_S("distributionType")->AddEllipsis());
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}































































