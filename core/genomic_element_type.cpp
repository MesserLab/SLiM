//
//  genomic_element_type.cpp
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


#include "genomic_element_type.h"


GenomicElementType::GenomicElementType(int p_genomic_element_type_id, std::vector<int> p_mutation_types, std::vector<double> p_mutation_fractions)
{
	genomic_element_type_id_ = p_genomic_element_type_id;
	mutation_types_ = p_mutation_types;
	mutation_fractions_ = p_mutation_fractions;  
	
	if (mutation_types_.size() != mutation_fractions_.size())
	{
		std::cerr << "ERROR (Initialize): mutation types and fractions have different sizes" << std::endl;
		exit(1);
	}
	
	// Prepare to randomly draw mutation types
	double A[mutation_types_.size()];
	
	for (int i = 0; i < mutation_types_.size(); i++)
		A[i] = mutation_fractions_[i];
	
	lookup_mutation_type = gsl_ran_discrete_preproc(p_mutation_fractions.size(), A);
}

int GenomicElementType::DrawMutationType() const
{
	return mutation_types_[gsl_ran_discrete(g_rng, lookup_mutation_type)];
}

std::ostream &operator<<(std::ostream &p_outstream, const GenomicElementType &p_genomic_element_type)
{
	p_outstream << "GenomicElementType{mutation_types_ ";
	
	if (p_genomic_element_type.mutation_types_.size() == 0)
	{
		p_outstream << "*";
	}
	else
	{
		p_outstream << "<";
		
		for (int i = 0; i < p_genomic_element_type.mutation_types_.size(); ++i)
		{
			p_outstream << p_genomic_element_type.mutation_types_[i];
			
			if (i < p_genomic_element_type.mutation_types_.size() - 1)
				p_outstream << " ";
		}
		
		p_outstream << ">";
	}
	
	p_outstream << ", mutation_fractions_ ";
	
	if (p_genomic_element_type.mutation_fractions_.size() == 0)
	{
		p_outstream << "*";
	}
	else
	{
		p_outstream << "<";
		
		for (int i = 0; i < p_genomic_element_type.mutation_fractions_.size(); ++i)
		{
			p_outstream << p_genomic_element_type.mutation_fractions_[i];
			
			if (i < p_genomic_element_type.mutation_fractions_.size() - 1)
				p_outstream << " ";
		}
		
		p_outstream << ">";
	}

	p_outstream << "}";
	
	return p_outstream;
}



































































