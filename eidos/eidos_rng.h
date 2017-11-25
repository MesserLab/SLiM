//
//  eidos_rng.h
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

/*
 
 Eidos uses a globally shared random number generator called gEidos_rng.  This file defines that global and relevant helper functions.
 
 */

#ifndef __Eidos__eidos_rng__
#define __Eidos__eidos_rng__


#if 1

// To build with the GSL code included inside SLiM, use this branch, which includes the local GSL headers.  Do not
// include the GSL's include paths in the project's header search paths, and do not link with -lgsl or lgslcblas.
// See the _README file in the gsl directory for information on the local copy of the GSL included in this project.
#include "gsl_rng.h"
#include "gsl_randist.h"

#else

// To build with an externally built GSL library, use this branch, which includes GSL headers from the linked library.
//
// - compile with "Header Search Paths" : /opt/local/include and /usr/local/include
// - compile with "Library Search Paths" : /opt/local/lib and /usr/local/lib
// - link with "Other Linker Flags" : -lgsl and -lgslcblas.
//
// This should work with a GSL installed by either Macports or Homebrew on OS X.  On other platforms, use
// gsl-config --cflags --libs to get the correct header and library search paths.  You will likely want to
// remove the GSL folder inside this project, too, so there are no conflicts.
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#endif


#include <stdint.h>
#include <cmath>
#include "eidos_global.h"


// This is a globally shared random number generator.  Note that the globals for random bit generation below are also
// considered to be part of the RNG state; if the Context plays games with swapping different RNGs in and out, those
// globals need to get swapped as well.  Likewise for the last seed value; this is part of the RNG state in Eidos.
extern gsl_rng *gEidos_rng;
extern int gEidos_random_bool_bit_counter;
extern uint32_t gEidos_random_bool_bit_buffer;
extern unsigned long int gEidos_rng_last_seed;				// unsigned long int is the type used by gsl_rng_set()


// generate a new random number seed from the PID and clock time
unsigned long int Eidos_GenerateSeedFromPIDAndTime(void);

// set up the random number generator with a given seed
void Eidos_InitializeRNGFromSeed(unsigned long int p_seed);


// get a random bool from a random number generator
//static inline bool Eidos_RandomBool(gsl_rng * r) { return (bool)(gsl_rng_get(r) & 0x01); }

// optimization of this is possible assuming each bit returned by gsl_rng_get() is independent and usable as a random boolean.
// I can't find a hard guarantee of this for gsl_rng_taus2, but it is generally true of good modern RNGs...
static inline __attribute__((always_inline)) bool Eidos_RandomBool(gsl_rng *p_r)
{
	bool retval;
	
	if (gEidos_random_bool_bit_counter > 0)
	{
		gEidos_random_bool_bit_counter--;
		gEidos_random_bool_bit_buffer >>= 1;
		retval = gEidos_random_bool_bit_buffer & 0x01;
	}
	else
	{
		gEidos_random_bool_bit_buffer = (uint32_t)gsl_rng_get(p_r);	// gsl_rng_taus2 is in fact limited to unsigned 32-bit, according to its docs
		retval = gEidos_random_bool_bit_buffer & 0x01;
		gEidos_random_bool_bit_counter = 31;				// 32 good bits originally, and now we've used one
	}
	
	return retval;
}


// the gsl_rng_uniform_int() function is very slow, so this is a customized version that should be faster
// basically it is faster because (1) the range of the taus2 generator is hard-coded, (2) the range check
// is done only on #if DEBUG, and (3) it uses uint32; otherwise the logic is the same.
inline uint32_t Eidos_RandomInt(gsl_rng *p_r, uint32_t p_n)
{
	uint32_t scale = UINT32_MAX / p_n;
	uint32_t k;
	
#if DEBUG
	if ((p_n > INT32_MAX) || (p_n <= 0)) 
	{
		GSL_ERROR_VAL ("invalid n, either 0 or exceeds maximum value of generator", GSL_EINVAL, 0) ;
	}
#endif
	
	do
	{
		k = ((uint32_t)((p_r->type->get) (p_r->state))) / scale;
	}
	while (k >= p_n);
	
	return k;
}



// Fast Poisson drawing, usable when mu is small; algorithm from Wikipedia, referenced to Luc Devroye,
// Non-Uniform Random Variate Generation (Springer-Verlag, New York, 1986), chapter 10, page 505.
// The GSL Poisson code does not allow us to precalculate the exp() value, it is more than three times
// slower for some mu values, it doesn't let us force a non-zero draw, and it is not inlined (without
// modifying the GSL code), so there are good reasons for us to roll our own.  However, our version is
// safe only for mu < ~720, and the GSL's version is faster for mu > 250 anyway, so we cross over
// to using the GSL for mu > 250.  This is done on a per-draw basis.

// If someone does not trust our Poisson code, they can define USE_GSL_POISSON at compile time
// (i.e., -D USE_GSL_POISSON) to use the gsl_ran_poisson() instead.  It does make a substantial
// speed difference, though, so that option is turned off by default.  Note that at present the
// rpois() Eidos function always uses the GSL in any case.

//#define USE_GSL_POISSON

#ifndef USE_GSL_POISSON

static inline __attribute__((always_inline)) unsigned int Eidos_FastRandomPoisson(double p_mu)
{
	// Defer to the GSL for large values of mu; see comments above.
	if (p_mu > 250)
		return gsl_ran_poisson(gEidos_rng, p_mu);
	
	unsigned int x = 0;
	double p = exp(-p_mu);
	double s = p;
	double u = gsl_rng_uniform(gEidos_rng);
	
	while (u > s)
	{
		++x;
		p *= (p_mu / x);
		s += p;
		
		// If p_mu is too large (> ~720), this loop can hang as p underflows to zero.
	}
	
	return x;
}

// This version allows the caller to supply a precalculated exp(-mu) value
static inline __attribute__((always_inline)) unsigned int Eidos_FastRandomPoisson(double p_mu, double p_exp_neg_mu)
{
	// Defer to the GSL for large values of mu; see comments above.
	if (p_mu > 250)
		return gsl_ran_poisson(gEidos_rng, p_mu);
	
	// Test consistency; normally this is commented out
	//if (p_exp_neg_mu != exp(-p_mu))
	//	EIDOS_TERMINATION << "ERROR (Eidos_FastRandomPoisson): p_exp_neg_mu incorrect." << EidosTerminate(nullptr);
	
	unsigned int x = 0;
	double p = p_exp_neg_mu;
	double s = p;
	double u = gsl_rng_uniform(gEidos_rng);
	
	while (u > s)
	{
		++x;
		p *= (p_mu / x);
		s += p;
		
		// If p_mu is too large (> ~720), this loop can hang as p underflows to zero.
	}
	
	return x;
}

// This version specifies that the count is guaranteed not to be zero; zero has been ruled out by a previous test
static inline __attribute__((always_inline)) unsigned int Eidos_FastRandomPoisson_NONZERO(double p_mu, double p_exp_neg_mu)
{
	// Defer to the GSL for large values of mu; see comments above.
	if (p_mu > 250)
	{
		unsigned int result;
		
		do
		{
			result = gsl_ran_poisson(gEidos_rng, p_mu);
		}
		while (result == 0);
		
		return result;
	}
	
	// Test consistency; normally this is commented out
	//if (p_exp_neg_mu != exp(-p_mu))
	//	EIDOS_TERMINATION << "ERROR (Eidos_FastRandomPoisson_NONZERO): p_exp_neg_mu incorrect." << EidosTerminate(nullptr);
	
	unsigned int x = 0;
	double p = p_exp_neg_mu;
	double s = p;
	double u = gsl_rng_uniform_pos(gEidos_rng);	// exclude 0.0 so u != s after rescaling
	
	// rescale u so that (u > s) is true in the first round
	u = u * (1.0 - s) + s;
	
	// do the first round, since we now know u > s
	++x;
	p *= p_mu;	// divided by x, but x is now 1
	s += p;
	
	while (u > s)
	{
		++x;
		p *= (p_mu / x);
		s += p;
		
		// If p_mu is too large (> ~720), this loop can hang as p underflows to zero.
	}
	
	return x;
}

double Eidos_FastRandomPoisson_PRECALCULATE(double p_mu);	// exp(-mu); can underflow to zero, in which case the GSL will be used


#endif // USE_GSL_POISSON


#endif /* defined(__Eidos__eidos_rng__) */




































































