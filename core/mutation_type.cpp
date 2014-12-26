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

#include "mutation_type.h"
#include "g_rng.h"
#include "stacktrace.h"


using std::cerr;
using std::endl;
using std::string;


bool MutationType::s_log_copy_and_assign_ = true;


MutationType::MutationType(const MutationType& p_original)
{
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* Subpopulation::Subpopulation(Subpopulation&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
	
	mutation_type_id_ = p_original.mutation_type_id_;
	dominance_coeff_ = p_original.dominance_coeff_;
	dfe_type_ = p_original.dfe_type_;
	dfe_parameters_ = p_original.dfe_parameters_;
}

MutationType& MutationType::operator= (const MutationType& p_original)
{
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* Subpopulation::operator=(Subpopulation&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
	
	mutation_type_id_ = p_original.mutation_type_id_;
	dominance_coeff_ = p_original.dominance_coeff_;
	dfe_type_ = p_original.dfe_type_;
	dfe_parameters_ = p_original.dfe_parameters_;
	
	return *this;
}

bool MutationType::LogMutationTypeCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}

MutationType::MutationType(int p_mutation_type_id, double p_dominance_coeff, char p_dfe_type, std::vector<double> p_dfe_parameters)
{
	mutation_type_id_ = p_mutation_type_id;
	
	dominance_coeff_ = static_cast<float>(p_dominance_coeff);
	dfe_type_ = p_dfe_type;
	dfe_parameters_ = p_dfe_parameters;
	
	static string possible_dfe_types = "fge";
	
	if (possible_dfe_types.find(dfe_type_) == string::npos)
	{
		cerr << "ERROR (Initialize): invalid mutation type '" << dfe_type_ << "'" << endl;
		exit(1);
	}
	
	if (dfe_parameters_.size() == 0)
	{
		cerr << "ERROR (Initialize): invalid mutation type parameters" << endl;
		exit(1);
	}
}

double MutationType::DrawSelectionCoefficient() const
{
	switch (dfe_type_)
	{
		case 'f': return dfe_parameters_[0];
		case 'g': return gsl_ran_gamma(g_rng, dfe_parameters_[1], dfe_parameters_[0] / dfe_parameters_[1]);
		case 'e': return gsl_ran_exponential(g_rng, dfe_parameters_[0]);
		default: exit(1);
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


































































