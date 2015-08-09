//
//  mutation_type.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
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


#include <iostream>
#include <gsl/gsl_randist.h>
#include <sstream>

#include "mutation_type.h"
#include "eidos_rng.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"


using std::endl;
using std::string;


#ifdef SLIMGUI
MutationType::MutationType(int p_mutation_type_id, double p_dominance_coeff, char p_dfe_type, std::vector<double> p_dfe_parameters, int p_mutation_type_index) :
	mutation_type_id_(p_mutation_type_id), dominance_coeff_(static_cast<typeof(dominance_coeff_)>(p_dominance_coeff)), dfe_type_(p_dfe_type), dfe_parameters_(p_dfe_parameters), mutation_type_index_(p_mutation_type_index)
#else
MutationType::MutationType(int p_mutation_type_id, double p_dominance_coeff, char p_dfe_type, std::vector<double> p_dfe_parameters) :
	mutation_type_id_(p_mutation_type_id), dominance_coeff_(static_cast<typeof(dominance_coeff_)>(p_dominance_coeff)), dfe_type_(p_dfe_type), dfe_parameters_(p_dfe_parameters)
#endif
{
	static string possible_dfe_types = "fge";
	
	if (possible_dfe_types.find(dfe_type_) == string::npos)
		EIDOS_TERMINATION << "ERROR (Initialize): invalid mutation type '" << dfe_type_ << "'" << eidos_terminate();
	if (dfe_parameters_.size() == 0)
		EIDOS_TERMINATION << "ERROR (Initialize): invalid mutation type parameters" << eidos_terminate();
}

MutationType::~MutationType(void)
{
	if (self_symbol_)
	{
		delete self_symbol_->second;
		delete self_symbol_;
	}
	
	if (cached_value_muttype_id_)
		delete cached_value_muttype_id_;
}

double MutationType::DrawSelectionCoefficient() const
{
	switch (dfe_type_)
	{
		case 'f': return dfe_parameters_[0];
		case 'g': return gsl_ran_gamma(gEidos_rng, dfe_parameters_[1], dfe_parameters_[0] / dfe_parameters_[1]);
		case 'e': return gsl_ran_exponential(gEidos_rng, dfe_parameters_[0]);
		default: EIDOS_TERMINATION << "ERROR (DrawSelectionCoefficient): invalid DFE type" << eidos_terminate(); return 0;
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
		
		for (int i = 0; i < p_mutation_type.dfe_parameters_.size(); ++i)
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
// Eidos support
//

void MutationType::GenerateCachedSymbolTableEntry(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	std::ostringstream mut_type_stream;
	
	mut_type_stream << "m" << mutation_type_id_;
	
	self_symbol_ = new EidosSymbolTableEntry(mut_type_stream.str(), (new EidosValue_Object_singleton_const(this))->SetExternalPermanent());
}

const EidosObjectClass *MutationType::Class(void) const
{
	return gSLiM_MutationType_Class;
}

void MutationType::Print(std::ostream &p_ostream) const
{
	p_ostream << *Class()->ElementType() << "<m" << mutation_type_id_ << ">";
}

EidosValue *MutationType::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:
		{
			// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
			// live for at least as long as the symbol table it may be placed into!
			if (!cached_value_muttype_id_)
				cached_value_muttype_id_ = (new EidosValue_Int_singleton_const(mutation_type_id_))->SetExternalPermanent();
			return cached_value_muttype_id_;
		}
		case gID_distributionType:
			return new EidosValue_String(std::string(1, dfe_type_));
		case gID_distributionParams:
			return new EidosValue_Float_vector(dfe_parameters_);
			
			// variables
		case gID_dominanceCoeff:
			return new EidosValue_Float_singleton_const(dominance_coeff_);
		case gID_tag:
			return new EidosValue_Int_singleton_const(tag_value_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void MutationType::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_dominanceCoeff:
		{
			double value = p_value->FloatAtIndex(0);
			
			dominance_coeff_ = static_cast<typeof(dominance_coeff_)>(value);	// float, at present, but I don't want to hard-code that
			return;
		}
			
		case gID_tag:
		{
			int64_t value = p_value->IntAtIndex(0);
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue *MutationType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	
	//
	//	*********************	- (void)setDistribution(string$ distributionType, ...)
	//
#pragma mark -setDistribution()
	
	if (p_method_id == gID_setDistribution)
	{
		string dfe_type_string = arg0_value->StringAtIndex(0);
		int expected_dfe_param_count = 0;
		std::vector<double> dfe_parameters;
		
		if (dfe_type_string.compare("f") == 0)
			expected_dfe_param_count = 1;
		else if (dfe_type_string.compare("g") == 0)
			expected_dfe_param_count = 2;
		else if (dfe_type_string.compare("e") == 0)
			expected_dfe_param_count = 1;
		else
			EIDOS_TERMINATION << "ERROR (MutationType::ExecuteInstanceMethod): setDistribution() distributionType \"" << dfe_type_string << "must be \"f\", \"g\", or \"e\"." << eidos_terminate();
		
		char dfe_type = dfe_type_string[0];
		
		if (p_argument_count != 1 + expected_dfe_param_count)
			EIDOS_TERMINATION << "ERROR (MutationType::ExecuteInstanceMethod): setDistribution() distributionType \"" << dfe_type << "\" requires exactly " << expected_dfe_param_count << " DFE parameter" << (expected_dfe_param_count == 1 ? "" : "s") << "." << eidos_terminate();
		
		for (int dfe_param_index = 0; dfe_param_index < expected_dfe_param_count; ++dfe_param_index)
			dfe_parameters.push_back(p_arguments[3 + dfe_param_index]->FloatAtIndex(0));
		
		// Everything seems to be in order, so replace our distribution info with the new info
		dfe_type_ = dfe_type;
		dfe_parameters_ = dfe_parameters;
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	MutationType_Class
//
#pragma mark MutationType_Class

class MutationType_Class : public EidosObjectClass
{
public:
	MutationType_Class(const MutationType_Class &p_original) = delete;	// no copy-construct
	MutationType_Class& operator=(const MutationType_Class&) = delete;	// no copying
	
	MutationType_Class(void);
	
	virtual const std::string *ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue *ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_MutationType_Class = new MutationType_Class();


MutationType_Class::MutationType_Class(void)
{
}

const std::string *MutationType_Class::ElementType(void) const
{
	return &gStr_MutationType;
}

const std::vector<const EidosPropertySignature *> *MutationType_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->push_back(SignatureForProperty(gID_id));
		properties->push_back(SignatureForProperty(gID_distributionType));
		properties->push_back(SignatureForProperty(gID_distributionParams));
		properties->push_back(SignatureForProperty(gID_dominanceCoeff));
		properties->push_back(SignatureForProperty(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *MutationType_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *distributionTypeSig = nullptr;
	static EidosPropertySignature *distributionParamsSig = nullptr;
	static EidosPropertySignature *dominanceCoeffSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					gID_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		distributionTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_distributionType,	gID_distributionType,	true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		distributionParamsSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_distributionParams,	gID_distributionParams,	true,	kEidosValueMaskFloat));
		dominanceCoeffSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_dominanceCoeff,		gID_dominanceCoeff,		false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		tagSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					gID_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:					return idSig;
		case gID_distributionType:		return distributionTypeSig;
		case gID_distributionParams:	return distributionParamsSig;
		case gID_dominanceCoeff:		return dominanceCoeffSig;
		case gID_tag:					return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *MutationType_Class::Methods(void) const
{
	std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->push_back(SignatureForMethod(gID_setDistribution));
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

EidosValue *MutationType_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}
































































