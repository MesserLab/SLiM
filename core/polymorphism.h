//
//  polymorphism.h
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

#ifndef __SLiM__polymorphism__
#define __SLiM__polymorphism__


#include <iostream>

#include "chromosome.h"


class polymorphism
{
public:
	
	int   id; // mutation id
	int   t;  // mutation type
	float s;  // selection coefficient
	int   i;  // subpopulation in which mutation arose
	int   g;  // generation in which mutation arose  
	int   n;  // prevalence
	
	polymorphism(int ID, int T, float S, int I, int G, int N);
	
	void print(int x, chromosome& chr);
	
	void print(std::ofstream& outfile, int x, chromosome& chr);
	
	void print_no_id(int x, chromosome& chr);
};


#endif /* defined(__SLiM__polymorphism__) */
