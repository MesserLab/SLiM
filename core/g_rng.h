//
//  g_rng.h
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

/*
 
 SLiM uses a globally shared random number generator called g_rng.  This file defines that global and relevant helper functions.
 
 */

#ifndef __SLiM__g_rng__
#define __SLiM__g_rng__


#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>


// this is a globally shared random number generator
extern const gsl_rng *g_rng; 

// generate a new random number seed from the PID and clock time
int GenerateSeedFromPIDAndTime();

// set up the random number generator with a given seed
void InitializeRNGFromSeed(int p_seed);


#endif /* defined(__SLiM__g_rng__) */




































































