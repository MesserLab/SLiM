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


#include "mutation_type.h"
#include "g_rng.h"

#include <iostream>
#include <gsl/gsl_randist.h>


mutation_type::mutation_type(float H, char D, std::vector<double> P)
{
	h = H;
	d = D;
	p = P;
	
	std::string s = "fge";
	
	if (s.find(d)==std::string::npos)  { std::cerr << "ERROR (initialize): invalid mutation type parameters" << std::endl; exit(1); }
	if (p.size()==0)              { std::cerr << "ERROR (initialize): invalid mutation type parameters" << std::endl; exit(1); }
}

float mutation_type::draw_s()
{
	switch (d)
	{
		case 'f': return p[0]; 
		case 'g': return gsl_ran_gamma(g_rng,p[1],p[0]/p[1]);
		case 'e': return gsl_ran_exponential(g_rng,p[0]);
		default: exit(1);
	}
}
