//
//  mutation_type.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_eidos_block.h"
#include "slim_sim.h"	// we need to tell the simulation if a DFE is set to non-neutral...

#include <iostream>
#include <sstream>
#include <algorithm>


using std::endl;
using std::string;


// stream output for enumerations
std::ostream& operator<<(std::ostream& p_out, DFEType p_dfe_type)
{
	switch (p_dfe_type)
	{
		case DFEType::kFixed:			p_out << gStr_f; break;
		case DFEType::kGamma:			p_out << gStr_g; break;
		case DFEType::kExponential:		p_out << gStr_e; break;
		case DFEType::kNormal:			p_out << gStr_n; break;
		case DFEType::kWeibull:			p_out << gStr_w; break;
	}
	
	return p_out;
}


#ifdef SLIMGUI
MutationType::MutationType(slim_objectid_t p_mutation_type_id, double p_dominance_coeff, DFEType p_dfe_type, std::vector<double> p_dfe_parameters, int p_mutation_type_index) : mutation_type_index_(p_mutation_type_index),
#else
MutationType::MutationType(slim_objectid_t p_mutation_type_id, double p_dominance_coeff, DFEType p_dfe_type, std::vector<double> p_dfe_parameters) :
#endif
	mutation_type_id_(p_mutation_type_id), dominance_coeff_(static_cast<slim_selcoeff_t>(p_dominance_coeff)), dfe_type_(p_dfe_type), dfe_parameters_(p_dfe_parameters), convert_to_substitution_(true), stack_policy_(MutationStackPolicy::kStack),
	self_symbol_(EidosGlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('m', p_mutation_type_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_MutationType_Class)))
{
	if (dfe_parameters_.size() == 0)
		EIDOS_TERMINATION << "ERROR (MutationType::MutationType): invalid mutation type parameters." << eidos_terminate();
	// intentionally no bounds checks for DFE parameters; the count of DFE parameters is checked prior to construction
	// intentionally no bounds check for dominance_coeff_
}

MutationType::~MutationType(void)
{
}

double MutationType::DrawSelectionCoefficient(void) const
{
	switch (dfe_type_)
	{
		case DFEType::kFixed:			return dfe_parameters_[0];
		case DFEType::kGamma:			return gsl_ran_gamma(gEidos_rng, dfe_parameters_[1], dfe_parameters_[0] / dfe_parameters_[1]);
		case DFEType::kExponential:		return gsl_ran_exponential(gEidos_rng, dfe_parameters_[0]);
		case DFEType::kNormal:			return gsl_ran_gaussian(gEidos_rng, dfe_parameters_[1]) + dfe_parameters_[0];
		case DFEType::kWeibull:			return gsl_ran_weibull(gEidos_rng, dfe_parameters_[0], dfe_parameters_[1]);
	}
}

std::ostream &operator<<(std::ostream &p_outstream, const MutationType &p_mutation_type)
{
	p_outstream << "MutationType{dominance_coeff_ " << p_mutation_type.dominance_coeff_ << ", dfe_type_ '" << p_mutation_type.dfe_type_ << "', dfe_parameters_ ";
	
	if (p_mutation_type.dfe_parameters_.size() == 0)
	{
		p_outstream << "*";
	}
	else
	{
		p_outstream << "<";
		
		for (unsigned int i = 0; i < p_mutation_type.dfe_parameters_.size(); ++i)
		{
			p_outstream << p_mutation_type.dfe_parameters_[i];
			
			if (i < p_mutation_type.dfe_parameters_.size() - 1)
				p_outstream << " ";
		}
		
		p_outstream << ">";
	}
	
	p_outstream << "}";
	
	return p_outstream;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

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
		case gID_id:
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
			
			if (!static_dfe_string_f)
			{
				static_dfe_string_f = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_f));
				static_dfe_string_g = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_g));
				static_dfe_string_e = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_e));
				static_dfe_string_n = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_n));
				static_dfe_string_w = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_w));
			}
			
			switch (dfe_type_)
			{
				case DFEType::kFixed:			return static_dfe_string_f;
				case DFEType::kGamma:			return static_dfe_string_g;
				case DFEType::kExponential:		return static_dfe_string_e;
				case DFEType::kNormal:			return static_dfe_string_n;
				case DFEType::kWeibull:			return static_dfe_string_w;
			}
		}
		case gID_distributionParams:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(dfe_parameters_));
			
			// variables
		case gID_convertToSubstitution:
			return (convert_to_substitution_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_dominanceCoeff:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(dominance_coeff_));
		case gID_mutationStackPolicy:
		{
			static EidosValue_SP static_policy_string_s;
			static EidosValue_SP static_policy_string_f;
			static EidosValue_SP static_policy_string_l;
			
			if (!static_policy_string_s)
			{
				static_policy_string_s = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_s));
				static_policy_string_f = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_f));
				static_policy_string_l = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_l));
			}
			
			switch (stack_policy_)
			{
				case MutationStackPolicy::kStack:		return static_policy_string_s;
				case MutationStackPolicy::kKeepFirst:	return static_policy_string_f;
				case MutationStackPolicy::kKeepLast:	return static_policy_string_l;
			}
		}
		case gID_tag:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void MutationType::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_convertToSubstitution:
		{
			eidos_logical_t value = p_value.LogicalAtIndex(0, nullptr);
			
			convert_to_substitution_ = value;
			return;
		}
			
		case gID_dominanceCoeff:
		{
			double value = p_value.FloatAtIndex(0, nullptr);
			
			dominance_coeff_ = static_cast<slim_selcoeff_t>(value);		// intentionally no bounds check
			return;
		}
			
		case gID_mutationStackPolicy:
		{
			std::string value = p_value.StringAtIndex(0, nullptr);
			
			if (value.compare(gStr_s) == 0)
				stack_policy_ = MutationStackPolicy::kStack;
			else if (value.compare(gStr_f) == 0)
				stack_policy_ = MutationStackPolicy::kKeepFirst;
			else if (value.compare(gStr_l) == 0)
				stack_policy_ = MutationStackPolicy::kKeepLast;
			else
				EIDOS_TERMINATION << "ERROR (MutationType::SetProperty): new value for property " << StringForEidosGlobalStringID(p_property_id) << " must be \"s\", \"f\", or \"l\"." << eidos_terminate();
			
			return;
		}
			
		case gID_tag:
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

EidosValue_SP MutationType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	
	//
	//	*********************	- (void)setDistribution(string$ distributionType, ...)
	//
#pragma mark -setDistribution()
	
	if (p_method_id == gID_setDistribution)
	{
		string dfe_type_string = arg0_value->StringAtIndex(0, nullptr);
		DFEType dfe_type;
		int expected_dfe_param_count = 0;
		std::vector<double> dfe_parameters;
		
		if (dfe_type_string.compare(gStr_f) == 0)
		{
			dfe_type = DFEType::kFixed;
			expected_dfe_param_count = 1;
		}
		else if (dfe_type_string.compare(gStr_g) == 0)
		{
			dfe_type = DFEType::kGamma;
			expected_dfe_param_count = 2;
		}
		else if (dfe_type_string.compare(gStr_e) == 0)
		{
			dfe_type = DFEType::kExponential;
			expected_dfe_param_count = 1;
		}
		else if (dfe_type_string.compare(gStr_n) == 0)
		{
			dfe_type = DFEType::kNormal;
			expected_dfe_param_count = 2;
		}
		else if (dfe_type_string.compare(gStr_w) == 0)
		{
			dfe_type = DFEType::kWeibull;
			expected_dfe_param_count = 2;
		}
		else
			EIDOS_TERMINATION << "ERROR (MutationType::ExecuteInstanceMethod): setDistribution() distributionType \"" << dfe_type_string << "\" must be \"f\", \"g\", \"e\", \"n\", or \"w\"." << eidos_terminate();
		
		if (p_argument_count != 1 + expected_dfe_param_count)
			EIDOS_TERMINATION << "ERROR (MutationType::ExecuteInstanceMethod): setDistribution() distributionType \"" << dfe_type << "\" requires exactly " << expected_dfe_param_count << " DFE parameter" << (expected_dfe_param_count == 1 ? "" : "s") << "." << eidos_terminate();
		
		for (int dfe_param_index = 0; dfe_param_index < expected_dfe_param_count; ++dfe_param_index)
		{
			EidosValue *dfe_param_value = p_arguments[1 + dfe_param_index].get();
			EidosValueType dfe_param_type = dfe_param_value->Type();
			
			if ((dfe_param_type != EidosValueType::kValueFloat) && (dfe_param_type != EidosValueType::kValueInt))
				EIDOS_TERMINATION << "ERROR (MutationType::ExecuteInstanceMethod): setDistribution() requires that DFE parameters be numeric (integer or float)." << eidos_terminate();
			
			dfe_parameters.emplace_back(dfe_param_value->FloatAtIndex(0, nullptr));
			// intentionally no bounds checks for DFE parameters
		}
		
		// Everything seems to be in order, so replace our distribution info with the new info
		dfe_type_ = dfe_type;
		dfe_parameters_ = dfe_parameters;
		
		// check whether we are now using a DFE type that is non-neutral; check and set pure_neutral_
		if ((dfe_type_ != DFEType::kFixed) || (dfe_parameters_[0] != 0.0))
		{
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
			
			if (sim)
				sim->pure_neutral_ = false;
		}
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	MutationType_Class
//
#pragma mark -
#pragma mark MutationType_Class

class MutationType_Class : public EidosObjectClass
{
public:
	MutationType_Class(const MutationType_Class &p_original) = delete;	// no copy-construct
	MutationType_Class& operator=(const MutationType_Class&) = delete;	// no copying
	
	MutationType_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_MutationType_Class = new MutationType_Class();


MutationType_Class::MutationType_Class(void)
{
}

const std::string &MutationType_Class::ElementType(void) const
{
	return gStr_MutationType;
}

const std::vector<const EidosPropertySignature *> *MutationType_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_id));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_convertToSubstitution));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_distributionType));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_distributionParams));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_dominanceCoeff));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_mutationStackPolicy));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *MutationType_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *convertToSubstitutionSig = nullptr;
	static EidosPropertySignature *distributionTypeSig = nullptr;
	static EidosPropertySignature *distributionParamsSig = nullptr;
	static EidosPropertySignature *dominanceCoeffSig = nullptr;
	static EidosPropertySignature *mutationStackPolicySig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,						gID_id,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		convertToSubstitutionSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_convertToSubstitution,	gID_convertToSubstitution,	false,	kEidosValueMaskLogical | kEidosValueMaskSingleton));
		distributionTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_distributionType,		gID_distributionType,		true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		distributionParamsSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_distributionParams,		gID_distributionParams,		true,	kEidosValueMaskFloat));
		dominanceCoeffSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_dominanceCoeff,			gID_dominanceCoeff,			false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		mutationStackPolicySig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationStackPolicy,		gID_mutationStackPolicy,	false,	kEidosValueMaskString | kEidosValueMaskSingleton));
		tagSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,						gID_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:					return idSig;
		case gID_convertToSubstitution:	return convertToSubstitutionSig;
		case gID_distributionType:		return distributionTypeSig;
		case gID_distributionParams:	return distributionParamsSig;
		case gID_dominanceCoeff:		return dominanceCoeffSig;
		case gID_mutationStackPolicy:	return mutationStackPolicySig;
		case gID_tag:					return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *MutationType_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_setDistribution));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *MutationType_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setDistributionSig = nullptr;
	
	if (!setDistributionSig)
	{
		setDistributionSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setDistribution, kEidosValueMaskNULL))->AddString_S("distributionType")->AddEllipsis();
	}
	
	if (p_method_id == gID_setDistribution)
		return setDistributionSig;
	else
		return EidosObjectClass::SignatureForMethod(p_method_id);
}

EidosValue_SP MutationType_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}
































































