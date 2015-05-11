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
		SLIM_TERMINATION << "ERROR (Initialize): invalid mutation type '" << dfe_type_ << "'" << endl << slim_terminate();
	if (dfe_parameters_.size() == 0)
		SLIM_TERMINATION << "ERROR (Initialize): invalid mutation type parameters" << endl << slim_terminate();
}

double MutationType::DrawSelectionCoefficient() const
{
	switch (dfe_type_)
	{
		case 'f': return dfe_parameters_[0];
		case 'g': return gsl_ran_gamma(g_rng, dfe_parameters_[1], dfe_parameters_[0] / dfe_parameters_[1]);
		case 'e': return gsl_ran_exponential(g_rng, dfe_parameters_[0]);
		default: SLIM_TERMINATION << "ERROR (DrawSelectionCoefficient): invalid DFE type" << endl << slim_terminate(); return 0;
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

#ifndef SLIMCORE
//
// SLiMscript support
//
std::string MutationType::ElementType(void) const
{
	return "MutationType";
}

void MutationType::Print(std::ostream &p_ostream) const
{
	p_ostream << ElementType() << "<m" << mutation_type_id_ << ">";
}

std::vector<std::string> MutationType::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("id");						// mutation_type_id_
	constants.push_back("distributionType");		// dfe_type_
	constants.push_back("distributionParams");		// dfe_parameters_
	
	return constants;
}

std::vector<std::string> MutationType::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back("dominanceCoeff");		// dominance_coeff_
	
	return variables;
}

ScriptValue *MutationType::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("id") == 0)
		return new ScriptValue_Int(mutation_type_id_);
	if (p_member_name.compare("distributionType") == 0)
		return new ScriptValue_String(std::string(1, dfe_type_));
	if (p_member_name.compare("distributionParams") == 0)
		return new ScriptValue_Float(dfe_parameters_);
	
	// variables
	if (p_member_name.compare("dominanceCoeff") == 0)
		return new ScriptValue_Float(dominance_coeff_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void MutationType::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	if (p_member_name.compare("dominanceCoeff") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt);
		
		double value = p_value->FloatAtIndex(0);
		
		dominance_coeff_ = (int)value;
		return;
	}
	
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> MutationType::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	// setDistribution()
	
	return methods;
}

const FunctionSignature *MutationType::SignatureForMethod(std::string const &p_method_name) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *MutationType::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}

#endif	// #ifndef SLIMCORE































































