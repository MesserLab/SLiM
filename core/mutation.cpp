//
//  mutation.cpp
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


#include "mutation.h"


mutation::mutation(void) { ; }

mutation::mutation(int T, int X, float S, int I, int G) 
{ 
	t = T;
	x = X;
	s = S;
	i = I;
	g = G;
}

bool operator< (const mutation &M1,const mutation &M2)
{
	return M1.x < M2.x;
};

bool operator== (const mutation &M1,const mutation &M2)
{
	return (M1.x == M2.x && M1.t == M2.t && M1.s == M2.s);
};
