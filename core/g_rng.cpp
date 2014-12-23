//
//  g_rng.cpp
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


#include "g_rng.h"

#include <unistd.h>
#include <time.h>


const gsl_rng *g_rng; 


int GenerateSeedFromPIDAndTime()
{
	long pid = getpid();
	time_t t;
	
	time(&t);
	t += pid;
	
	return static_cast<int>(t);
}

void InitializeRNGFromSeed(int p_seed)
{
	g_rng = gsl_rng_alloc(gsl_rng_taus2);
	gsl_rng_set(g_rng, static_cast<long>(p_seed));
}

































































