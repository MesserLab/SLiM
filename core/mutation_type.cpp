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
#include "g_rng.h"
#include "slim_global.h"
#include "script_functionsignature.h"


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
		SLIM_TERMINATION << "ERROR (Initialize): invalid mutation type '" << dfe_type_ << "'" << slim_terminate();
	if (dfe_parameters_.size() == 0)
		SLIM_TERMINATION << "ERROR (Initialize): invalid mutation type parameters" << slim_terminate();
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
		case 'g': return gsl_ran_gamma(g_rng, dfe_parameters_[1], dfe_parameters_[0] / dfe_parameters_[1]);
		case 'e': return gsl_ran_exponential(g_rng, dfe_parameters_[0]);
		default: SLIM_TERMINATION << "ERROR (DrawSelectionCoefficient): invalid DFE type" << slim_terminate(); return 0;
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
// SLiMscript support
//

void MutationType::GenerateCachedSymbolTableEntry(void)
{
	std::ostringstream mut_type_stream;
	
	mut_type_stream << "m" << mutation_type_id_;
	
	self_symbol_ = new SymbolTableEntry(mut_type_stream.str(), (new ScriptValue_Object_singleton_const(this))->SetExternallyOwned());
}

const std::string MutationType::ElementType(void) const
{
	return gStr_MutationType;
}

void MutationType::Print(std::ostream &p_ostream) const
{
	p_ostream << ElementType() << "<m" << mutation_type_id_ << ">";
}

std::vector<std::string> MutationType::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_id);						// mutation_type_id_
	constants.push_back(gStr_distributionType);		// dfe_type_
	constants.push_back(gStr_distributionParams);		// dfe_parameters_
	
	return constants;
}

std::vector<std::string> MutationType::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back(gStr_dominanceCoeff);		// dominance_coeff_
	
	return variables;
}

bool MutationType::MemberIsReadOnly(GlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_id:
		case gID_distributionType:
		case gID_distributionParams:
			return true;
			
			// variables
		case gID_dominanceCoeff:
			return false;
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::MemberIsReadOnly(p_member_id);
	}
}

ScriptValue *MutationType::GetValueForMember(GlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
			// constants
		case gID_id:
		{
			if (!cached_value_muttype_id_)
				cached_value_muttype_id_ = (new ScriptValue_Int_singleton_const(mutation_type_id_))->SetExternallyOwned();
			return cached_value_muttype_id_;
		}
		case gID_distributionType:
			return new ScriptValue_String(std::string(1, dfe_type_));
		case gID_distributionParams:
			return new ScriptValue_Float_vector(dfe_parameters_);
			
			// variables
		case gID_dominanceCoeff:
			return new ScriptValue_Float_singleton_const(dominance_coeff_);
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::GetValueForMember(p_member_id);
	}
}

void MutationType::SetValueForMember(GlobalStringID p_member_id, ScriptValue *p_value)
{
	if (p_member_id == gID_dominanceCoeff)
	{
		TypeCheckValue(__func__, p_member_id, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
		
		double value = p_value->FloatAtIndex(0);
		
		dominance_coeff_ = static_cast<typeof(dominance_coeff_)>(value);	// float, at present, but I don't want to hard-code that
		return;
	}
	
	return ScriptObjectElement::SetValueForMember(p_member_id, p_value);
}

std::vector<std::string> MutationType::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	methods.push_back(gStr_changeDistribution);
	
	return methods;
}

const FunctionSignature *MutationType::SignatureForMethod(GlobalStringID p_method_id) const
{
	static FunctionSignature *changeDistributionSig = nullptr;
	
	if (!changeDistributionSig)
	{
		changeDistributionSig = (new FunctionSignature(gStr_changeDistribution, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddString_S()->AddEllipsis();
	}
	
	if (p_method_id == gID_changeDistribution)
		return changeDistributionSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_id);
}

ScriptValue *MutationType::ExecuteMethod(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	ScriptValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	
	//
	//	*********************	- (void)changeDistribution(string$ distributionType, ...)
	//
#pragma mark -changeDistribution()
	
	if (p_method_id == gID_changeDistribution)
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
			SLIM_TERMINATION << "ERROR (MutationType::ExecuteMethod): changeDistribution() distributionType \"" << dfe_type_string << "must be \"f\", \"g\", or \"e\"." << slim_terminate();
		
		char dfe_type = dfe_type_string[0];
		
		if (p_argument_count != 1 + expected_dfe_param_count)
			SLIM_TERMINATION << "ERROR (MutationType::ExecuteMethod): changeDistribution() distributionType \"" << dfe_type << "\" requires exactly " << expected_dfe_param_count << " DFE parameter" << (expected_dfe_param_count == 1 ? "" : "s") << "." << slim_terminate();
		
		for (int dfe_param_index = 0; dfe_param_index < expected_dfe_param_count; ++dfe_param_index)
			dfe_parameters.push_back(p_arguments[3 + dfe_param_index]->FloatAtIndex(0));
		
		// Everything seems to be in order, so replace our distribution info with the new info
		dfe_type_ = dfe_type;
		dfe_parameters_ = dfe_parameters;
		
		return gStaticScriptValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return ScriptObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}
































































