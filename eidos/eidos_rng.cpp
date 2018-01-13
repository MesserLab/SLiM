//
//  eidos_rng.cpp
//  Eidos
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2017 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.


#include "eidos_rng.h"

#include <unistd.h>
#include <sys/time.h>


gsl_rng *gEidos_rng = nullptr;
int gEidos_random_bool_bit_counter = 0;
uint32_t gEidos_random_bool_bit_buffer = 0;
unsigned long int gEidos_rng_last_seed = 0;				// unsigned long int is the type used by gsl_rng_set()


unsigned long int Eidos_GenerateSeedFromPIDAndTime(void)
{
	static long int hereCounter = 0;
	pid_t pid = getpid();
	struct timeval te; 
	
	gettimeofday(&te, NULL); // get current time
	
	long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;	// calculate milliseconds
	
	milliseconds += (pid * 10000000);		// try to make the pid matter a lot, to separate runs made on different cores close in time
	milliseconds += (hereCounter++);
	
	return (unsigned long int)milliseconds;
}

void Eidos_InitializeRNGFromSeed(unsigned long int p_seed)
{
	// Allocate the RNG if needed
	if (!gEidos_rng)
		gEidos_rng = gsl_rng_alloc(gsl_rng_taus2);	// the assumption of taus2 is hard-coded in eidos_rng.h
	
	// Then set the seed as requested
	
	// BCH 12 Sept. 2016: it turns out that gsl_rng_taus2 produces exactly the same sequence for seeds 0 and 1.  This is obviously
	// undesirable; people will often do a set of runs with sequential seeds starting at 0 and counting up, and they will get
	// identical runs for 0 and 1.  There is no way to re-map the seed space to get rid of the problem altogether; all we can do
	// is shift it to a place where it is unlikely to cause a problem.  So that's what we do.
	if ((p_seed > 0) && (p_seed < 10000000000000000000UL))
		gsl_rng_set(gEidos_rng, p_seed + 1);	// map 1 -> 2, 2-> 3, 3-> 4, etc.
	else
		gsl_rng_set(gEidos_rng, p_seed);		// 0 stays 0
	
	// remember the seed as part of the RNG state
	
	// BCH 12 Sept. 2016: we want to return the user the same seed they requested, if they call getSeed(), so we save the requested
	// seed, not the seed shifted by one that is actually passed to the GSL above.
	gEidos_rng_last_seed = p_seed;
	
	// These need to be zeroed out, too; they are part of our RNG state
	gEidos_random_bool_bit_counter = 0;
	gEidos_random_bool_bit_buffer = 0;
}

#ifndef USE_GSL_POISSON
double Eidos_FastRandomPoisson_PRECALCULATE(double p_mu)
{
	// OK, so where does 720 come from?  Primarily, values much greater than that cause an underflow in the algorithm
	// we're using to do fast Poisson draws, so that's a showstopper.  Devroye cites Atkinson 1979 as using lookup tables
	// for mu >= 2, but my testing indicates that that is unnecessary for our purposes (see poisson_test.R).  Presumably
	// Atkinson is trying to avoid any numerical error whatsoever, within the limits of double precision, but we are not
	// that strict; as long as the Poisson draw distribution is close enough to give basically the right mutation and
	// recombination rates, tiny numerical errors are not important to us.  My testing with poisson_test.R indicates that
	// our error is very small even at the largest values of p_mu that we allow.
	
	// We no longer raise for mu > 720.  Instead, we defer to the GSL in that case, when the draw actually occurs.
	//
	//if (p_mu > 720)
	//	EIDOS_TERMINATION << "ERROR (Eidos_FastRandomPoisson_PRECALCULATE): rate for Poisson draws is too large; please compile SLiM with -D USE_GSL_POISSON if you really want to use a mutation or recombination rate this high." << EidosTerminate(nullptr);
	
	return exp(-p_mu);
}
#endif































































