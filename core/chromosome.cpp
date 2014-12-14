//
//  chromosome.cpp
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


#include "chromosome.h"

#include <iostream>

#include "g_rng.h"


void chromosome::initialize_rng()
{
	if (size() == 0)       { std::cerr << "ERROR (initialize): empty chromosome" << std::endl; exit(1); }
	if (rec_r.size() == 0) { std::cerr << "ERROR (initialize): recombination rate not specified" << std::endl; exit(1); }
	if (!(M>=0))           { std::cerr << "ERROR (initialize): invalid mutation rate" << std::endl; exit(1); }
	
	L = 0;
	
	for (int i=0; i<size(); i++)
	{
		if (genomic_element_types.count(operator[](i).i)==0) 
		{ 
			std::cerr << "ERROR (initialize): genomic element type " << operator[](i).i << " not defined" << std::endl; exit(1); 
		}
	}
	
	for (std::map<int,genomic_element_type>::iterator it = genomic_element_types.begin(); it!=genomic_element_types.end(); it++)
	{
		for (int j=0; j<it->second.m.size(); j++)
		{
			if (mutation_types.count(it->second.m[j]) == 0)
			{
				std::cerr << "ERROR (initialize): mutation type " << it->second.m[j] << " not defined" << std::endl; exit(1); 
			}
		}
	}  
	
	double A[size()]; int l = 0;
	for (int i=0; i<size(); i++) 
	{ 
		if (operator[](i).e > L) { L = operator[](i).e; }
		int l_i = operator[](i).e - operator[](i).s + 1.0; 
		A[i] = (double)l_i; l += l_i;
	}
	LT_M = gsl_ran_discrete_preproc(size(),A); M = M*(double)l;
	
	double B[rec_r.size()];
	B[0] = rec_r[0]*(double)rec_x[0]; R += B[0];
	for (int i=1; i<rec_r.size(); i++) 
	{ 
		B[i] = rec_r[i]*(double)(rec_x[i]-rec_x[i-1]); R+= B[i];
		if (rec_x[i]>L) { L = rec_x[i]; }
	}
	LT_R = gsl_ran_discrete_preproc(rec_r.size(),B);
}


int chromosome::draw_n_mut() { return gsl_ran_poisson(g_rng,M); }

mutation chromosome::draw_new_mut(int i, int g)
{
	int ge = gsl_ran_discrete(g_rng,LT_M); // genomic element
	genomic_element_type ge_type = genomic_element_types.find(operator[](ge).i)->second; // genomic element type
	
	int mut_type_id = ge_type.draw_mutation_type(); // mutation type id
	mutation_type mut_type = mutation_types.find(mut_type_id)->second; // mutation type
	
	int   x = operator[](ge).s + gsl_rng_uniform_int(g_rng,operator[](ge).e - operator[](ge).s + 1); // position    
	float s = mut_type.draw_s(); // selection coefficient
	
	return mutation(mut_type_id,x,s,i,g);
}


std::vector<int> chromosome::draw_breakpoints()
{
	vector<int> r;
	
	// draw recombination breakpoints
	
	int nr = gsl_ran_poisson(g_rng,R);
	
	for (int i=0; i<nr; i++)
	{
		int x = 0;
		int j = gsl_ran_discrete(g_rng,LT_R);
		
		if (j==0) { x = gsl_rng_uniform_int(g_rng,rec_x[j]); }
		else     { x = rec_x[j-1] + gsl_rng_uniform_int(g_rng,rec_x[j]-rec_x[j-1]); }
		
		r.push_back(x);
		
		if (gsl_rng_uniform(g_rng)<G_f) // recombination results in gene conversion 
		{
			int x2 = x+gsl_ran_geometric(g_rng,1.0/G_l);
			r.push_back(x2);
		}
	}
	
	return r;
}

