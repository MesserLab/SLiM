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
#include "math.h"


// this is a globally shared random number generator
extern const gsl_rng *g_rng; 


// generate a new random number seed from the PID and clock time
int GenerateSeedFromPIDAndTime();

// set up the random number generator with a given seed
void InitializeRNGFromSeed(int p_seed);


// get a random bool from a random number generator
//static inline bool g_rng_bool(const gsl_rng * r) { return (bool)(gsl_rng_get(r) & 0x01); }

// optimization of this is possible assuming each bit returned by gsl_rng_get() is independent and usable as a random boolean.
// I can't find a hard guarantee of this for gsl_rng_taus2, but it is generally true of good modern RNGs...
extern int g_random_bool_bit_counter;
extern unsigned long int g_random_bool_bit_buffer;

static inline __attribute__((always_inline)) bool g_rng_bool(const gsl_rng * r)
{
	bool retval;
	
	if (g_random_bool_bit_counter > 0)
	{
		g_random_bool_bit_counter--;
		g_random_bool_bit_buffer >>= 1;
		retval = g_random_bool_bit_buffer & 0x01;
	}
	else
	{
		g_random_bool_bit_buffer = gsl_rng_get(r);
		retval = g_random_bool_bit_buffer & 0x01;
		g_random_bool_bit_counter = 31;				// 32 good bits originally, and now we've used one
	}
	
	return retval;
}


// Fast Poisson drawing, usable when mu is small; algorithm from Wikipedia, referenced to Luc Devroye, Non-Uniform Random Variate Generation (Springer-Verlag, New York, 1986), chapter 10, page 505
// The expected number of mutations / breakpoints will always be quite small, so we should be safe using this algorithm.  The GSL Poisson draw code is similarly fast for very small mu, but it does not
// allow us to precalculate the exp() value, nor does it allow us to inline, so there are some good reasons for us to roll our own here.
static inline __attribute__((always_inline)) unsigned int slim_fast_ran_poisson(double mu)
{
	unsigned int x = 0;
	double p = exp(-mu);
	double s = p;
	double u = gsl_rng_uniform(g_rng);
	
	while (u > s)
	{
		++x;
		p *= (mu / x);
		s += p;
	}
	
	return x;
}

// This version allows the caller to supply a precalculated exp(-mu) value
static inline __attribute__((always_inline)) unsigned int slim_fast_ran_poisson(double mu, double exp_neg_mu)
{
	unsigned int x = 0;
	double p = exp_neg_mu;
	double s = p;
	double u = gsl_rng_uniform(g_rng);
	
	while (u > s)
	{
		++x;
		p *= (mu / x);
		s += p;
	}
	
	return x;
}

// This version specifies that the count is guaranteed not to be zero; zero has been ruled out by a previous test
static inline __attribute__((always_inline)) unsigned int slim_fast_ran_poisson_nonzero(double mu, double exp_neg_mu)
{
	unsigned int x = 0;
	double p = exp_neg_mu;
	double s = p;
	double u = gsl_rng_uniform_pos(g_rng);	// exclude 0.0 so u != s after rescaling
	
	// rescale u so that (u > s) is true in the first round
	u = u * (1.0 - s) + s;
	
	// do the first round, since we now know u > s
	++x;
	p *= mu;
	s += p;
	
	while (u > s)
	{
		++x;
		p *= (mu / x);
		s += p;
	}
	
	return x;
}


#endif /* defined(__SLiM__g_rng__) */




































































