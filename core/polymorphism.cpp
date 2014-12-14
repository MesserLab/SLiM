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


polymorphism::polymorphism(int ID, int T, float S, int I, int G, int N)
{
	id = ID;
	t  = T;
	s  = S;
	i  = I;
	g  = G;
	n  = N;
}

void polymorphism::print(int x, chromosome& chr) 
{ 
	float h = chr.mutation_types.find(t)->second.h;
	std::cout << id << " m" << t << " " << x+1 << " " << s << " " << h << " p" << i << " " << g << " " << n << std::endl; 
}

void polymorphism::print(std::ofstream& outfile, int x, chromosome& chr) 
{ 
	float h = chr.mutation_types.find(t)->second.h;
	outfile << id << " m" << t << " " << x+1 << " " << s << " " << h << " p" << i << " " << g << " " << n << std::endl; 
}

void polymorphism::print_no_id(int x, chromosome& chr) 
{ 
	float h = chr.mutation_types.find(t)->second.h;
	std::cout << "m" << t << " " << x+1 << " " << s << " " << h << " p" << i << " " << g << " " << n << std::endl; 
}
