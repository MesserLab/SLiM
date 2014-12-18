//
//  polymorphism.cpp
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


#include "polymorphism.h"

#include <fstream>


Polymorphism::Polymorphism(int p_mutation_id, int p_mutation_type, double p_selection_coeff, int p_subpop_index, int p_generation, int p_prevalence)
{
	mutation_id_ = p_mutation_id;
	mutation_type_  = p_mutation_type;
	selection_coeff_  = (float)p_selection_coeff;
	subpop_index_  = p_subpop_index;
	generation_  = p_generation;
	prevalence_  = p_prevalence;
}

void Polymorphism::print(int p_index, Chromosome &p_chromosome) 
{ 
	double dominance_coeff = p_chromosome.mutation_types_.find(mutation_type_)->second.dominance_coeff_;
	
	std::cout << mutation_id_ << " m" << mutation_type_ << " " << p_index + 1 << " " << selection_coeff_ << " " << dominance_coeff << " p" << subpop_index_ << " " << generation_ << " " << prevalence_ << std::endl; 
}

void Polymorphism::print(std::ofstream &p_outfile, int p_index, Chromosome &p_chromosome) 
{ 
	double dominance_coeff = p_chromosome.mutation_types_.find(mutation_type_)->second.dominance_coeff_;
	
	p_outfile << mutation_id_ << " m" << mutation_type_ << " " << p_index + 1 << " " << selection_coeff_ << " " << dominance_coeff << " p" << subpop_index_ << " " << generation_ << " " << prevalence_ << std::endl; 
}

void Polymorphism::print_no_id(int p_index, Chromosome &p_chromosome) 
{ 
	double dominance_coeff = p_chromosome.mutation_types_.find(mutation_type_)->second.dominance_coeff_;
	
	std::cout << "m" << mutation_type_ << " " << p_index + 1 << " " << selection_coeff_ << " " << dominance_coeff << " p" << subpop_index_ << " " << generation_ << " " << prevalence_ << std::endl; 
}




































































