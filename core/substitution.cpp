//
//  substitution.cpp
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


#include "substitution.h"

#include <iostream>


substitution::substitution(mutation M, int F)
{
	t = M.t;
	x = M.x;
	s = M.s;
	i = M.i;
	g = M.g;
	f = F;
}

void substitution::print(chromosome& chr) 
{ 
	float h = chr.mutation_types.find(t)->second.h;
	std::cout << " m" << t << " " << x+1 << " " << s << " " << h << " p" << i << " " << g << " "<< f << std::endl; 
}
