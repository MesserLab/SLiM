//
//  chromosome.h
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

#ifndef __SLiM__chromosome__
#define __SLiM__chromosome__


#include <vector>
#include <map>

#include "mutation.h"
#include "mutation_type.h"
#include "genomic_element.h"
#include "genomic_element_type.h"
#include "g_rng.h"


class chromosome : public std::vector<genomic_element>
{
	// the chromosome is a vector of genomic elements (type, start, end)
	
private:
	
	gsl_ran_discrete_t* LT_M; // mutation
	gsl_ran_discrete_t* LT_R; // recombination
	
public:
	
	std::map<int,mutation_type>        mutation_types;
	std::map<int,genomic_element_type> genomic_element_types;
	vector<int>                   rec_x;
	vector<double>                rec_r;
	
	int    L;   // length of chromosome
	double M;   // overall mutation rate
	double R;   // overall recombination rate
	double G_f; // gene conversion fraction
	double G_l; // average stretch length
	
	void initialize_rng();	
	
	int draw_n_mut();
	
	mutation draw_new_mut(int i, int g);
	
	vector<int> draw_breakpoints();
};


#endif /* defined(__SLiM__chromosome__) */
