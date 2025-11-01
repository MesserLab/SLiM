//
//  eidos_rng.h
//  Eidos
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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
 
 Eidos uses a globally shared random number generator.  This file defines that global and relevant helper
 functions.  Internally, it actually maintains a separate 32-bit and 64-bit generator seeded with the same
 value, for maximum speed when only 32 bits are needed.  When running multithreaded, there is one such
 generator setup per thread.
 
 */

#ifndef __Eidos__eidos_rng__
#define __Eidos__eidos_rng__


#include "gsl_rng.h"
#include "gsl_randist.h"

// BCH 1 November 2025: We now use the PCG random number generator.  See https://www.pcg-random.org.  This
// is licensed under the Apache License, Version 2.0, which is GPL 3.0 compatible, so we have included it in
// the project.  This was downloaded from the PCG website at https://www.pcg-random.org.  The original paper
// on the PCG generator is at https://www.cs.hmc.edu/tr/hmc-cs-2014-0905.pdf and can be cited as:
//
//	O’Neill, M. E. (2014). PCG: A family of simple fast space-efficient statistically good algorithms for
//		random number generation. Harvey Mudd College, Claremont, CA: HMC-CS-2014-0905.
//
#include "pcg_random.hpp"

#include <stdint.h>
#include <cmath>
#include <vector>
#include "eidos_globals.h"


// OK, so.  This header defines the Eidos random number generator, which is now a bit of a weird hybrid.
// We need to use the GSL's RNG for most purposes, because we want to use its random distributions and so
// forth.  However, we also want access to the RNG more directly, for greater speed; and we want to be
// able to generate both 32-bit draws (for maximum speed) and 64-bit draws (for greater range/precision).
// So we keep separate 32-bit and 64-bit generators seeded in the same way, and wrap the 64-bit generator
// with a GSL generator so that we can use it through GSL calls as well.  These typedefs should be used:
typedef pcg32_fast EidosRNG_32_bit;
typedef pcg64_fast EidosRNG_64_bit;

// This struct defines all of the variables associated with both RNGs; this is the complete Eidos RNG state.
typedef struct Eidos_RNG_State
{
	unsigned long int rng_last_seed_;		// unsigned long int is the type used for seeds in the GSL
	
	// We now use the pcg32_fast generator to get 32-bit random numbers for a few applications
	EidosRNG_32_bit pcg32_rng_;
	
	// We now use the pcg64_fast generator to get 64-bit random numbers for most purposes
	EidosRNG_64_bit pcg64_rng_;
	
	// Our GSL RNG simply wraps the pcg64_rng_ generator above, and contains an UNOWNED pointer to it
	gsl_rng gsl_rng_;
	
	// random coin-flip generator; this is based on the pcg64_rng_ generator now
	int random_bool_bit_counter_;
	uint64_t random_bool_bit_buffer_;
} Eidos_RNG_State;


// This is the globally shared random number generator.  Note that the globals for random bit generation below
// are also considered to be part of the RNG state; if the Context plays games with swapping different RNGs in
// and out, those globals need to get swapped as well.  Likewise for the last seed value; this is part of the
// RNG state in Eidos.
// BCH 11/5/2022: We now keep a single Eidos_RNG_State when running single-threaded, but when running
// multithreaded we keep one Eidos_RNG_State per thread.  This allows threads to get random numbers without
// any locking.  This means that each thread will have its own independent random number sequence, and makes
// the concept of a "seed" a bit nebulous; in fact, each thread's RNG will be seeded with a different value so
// that they do not all follow the same sequence.  The random number sequence when running multithreaded will
// not be reproducible; but it really can't be, since multithreading will divide tasks up unpredictably and
// execute out of linear sequence.
// BCH 12/26/2022: The per-thread RNGs are now allocated separately, on the thread that will use them, so they
// get kept in the best place in memory for that thread ("first touch").
extern bool gEidos_RNG_Initialized;

#ifndef _OPENMP
extern Eidos_RNG_State gEidos_RNG_SINGLE;
#else
extern std::vector<Eidos_RNG_State *> gEidos_RNG_PERTHREAD;
#endif

// Calls to the GSL should use these macros to get the RNG state they need, whether single- or multi-threaded.
// BCH 11/5/2022: The thread number must now be supplied.  It will be zero when single-threaded, and so is
// ignored.  Since this is now a bit more heavyweight, the RNG for a thread should be obtained outside of any
// core loops.  The most important thing is that when there is a parallel region, the RNG is obtained INSIDE
// that region!  These can be used as follows:
//
//	gsl_rng *rng_gsl = EIDOS_GSL_RNG(omp_get_thread_num());
//	EidosRNG_32_bit &rng_32 = EIDOS_32BIT_RNG(omp_get_thread_num());
//	EidosRNG_64_bit &rng_64 = EIDOS_64BIT_RNG(omp_get_thread_num());
//	Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
//
#ifndef _OPENMP
#define EIDOS_GSL_RNG(threadnum)	(&gEidos_RNG_SINGLE.gsl_rng_)
#define EIDOS_32BIT_RNG(threadnum)	(gEidos_RNG_SINGLE.pcg32_rng_)
#define EIDOS_64BIT_RNG(threadnum)	(gEidos_RNG_SINGLE.pcg64_rng_)
#define EIDOS_STATE_RNG(threadnum)	(&gEidos_RNG_SINGLE)
#else
#define EIDOS_GSL_RNG(threadnum)	(&gEidos_RNG_PERTHREAD[threadnum]->gsl_rng_)
#define EIDOS_32BIT_RNG(threadnum)	(gEidos_RNG_PERTHREAD[threadnum]->pcg32_rng_)
#define EIDOS_64BIT_RNG(threadnum)	(gEidos_RNG_PERTHREAD[threadnum]->pcg64_rng_)
#define EIDOS_STATE_RNG(threadnum)	(gEidos_RNG_PERTHREAD[threadnum])
#endif

#if DEBUG
#define RNG_INIT_CHECK() if (!gEidos_RNG_Initialized) EIDOS_TERMINATION << "ERROR (RNG_INIT_CHECK): (internal error) the Eidos random number generator is not initialized." << EidosTerminate(nullptr)
#else
#define RNG_INIT_CHECK()
#endif

// generate a new random number seed from the system - from /dev/random, or from PID and clock time, etc.
unsigned long int Eidos_GenerateRNGSeed(void);

// set up the random number generator with a given seed
// these first three functions are only for code that creates its own local temporary RNG
void _Eidos_InitializeOneRNG(Eidos_RNG_State &r);
void _Eidos_FreeOneRNG(Eidos_RNG_State &r);
void _Eidos_SetOneRNGSeed(Eidos_RNG_State &r, unsigned long int p_seed);

// these next three functions set up the shared RNG used by most clients, and handle separate RNGs per-thread
void Eidos_InitializeRNG(void);
void Eidos_FreeRNG(void);
void Eidos_SetRNGSeed(unsigned long int p_seed);

// generates an unsigned 32-bit integer -- uint32_t
inline __attribute__((always_inline)) uint32_t Eidos_rng_uniform_uint32(EidosRNG_32_bit &rng_32)
{
	RNG_INIT_CHECK();
	return rng_32();
}

// generates a signed 32-bit integer -- int32_t
inline __attribute__((always_inline)) int32_t Eidos_rng_uniform_int32(EidosRNG_32_bit &rng_32)
{
	RNG_INIT_CHECK();
	return static_cast<int32_t>(rng_32());
}

// generates an unsigned 64-bit integer -- uint64_t
inline __attribute__((always_inline)) uint64_t Eidos_rng_uniform_uint64(EidosRNG_64_bit &rng_64)
{
	RNG_INIT_CHECK();
	return rng_64();
}

// generates a signed 64-bit integer -- int64_t
inline __attribute__((always_inline)) int64_t Eidos_rng_uniform_int64(EidosRNG_64_bit &rng_64)
{
	RNG_INIT_CHECK();
	return static_cast<int64_t>(rng_64());
}

// generates a random double in [0,1] -- including 0 and 1 (Closed-Closed)
inline __attribute__((always_inline)) double Eidos_rng_uniform_doubleCC(EidosRNG_64_bit &rng_64)
{
	RNG_INIT_CHECK();
	return (rng_64() >> 11) * (1.0/9007199254740991.0);
}

// generates a random double in [0,1) -- including 0 but NOT 1 (Closed-Open)
inline __attribute__((always_inline)) double Eidos_rng_uniform_doubleCO(EidosRNG_64_bit &rng_64)
{
	RNG_INIT_CHECK();
	return (rng_64() >> 11) * (1.0/9007199254740992.0);
}

// generates a random double in (0,1) -- including NEITHER 0 nor 1 (Open-Open)
inline __attribute__((always_inline)) double Eidos_rng_uniform_doubleOO(EidosRNG_64_bit &rng_64)
{
	RNG_INIT_CHECK();
	return ((rng_64() >> 12) + 0.5) * (1.0/4503599627370496.0);
}


// generates a random unsigned 32-bit integer in the interval [0, p_n - 1]
inline __attribute__((always_inline)) uint32_t Eidos_rng_interval_uint32(EidosRNG_32_bit &rng_32, uint32_t p_n)
{
	// The gsl_rng_uniform_int() function is very slow, so this is a customized version that should be faster.
	// Basically it is faster because (1) the range of the generator is hard-coded, (2) the range check is done
	// only on #if DEBUG, and (3) it calls the generator directly, not through the GSL's pointer; otherwise the
	// logic is the same.
	uint32_t scale = UINT32_MAX / p_n;
	uint32_t k;
	
#if DEBUG
	if ((p_n > INT32_MAX) || (p_n <= 0)) 
	{
		GSL_ERROR_VAL("invalid n, either 0 or exceeds maximum value of generator", GSL_EINVAL, 0);
	}
#endif
	
	do
	{
		k = rng_32() / scale;
	}
	while (k >= p_n);
	
	return k;
}

// generates a random unsigned 64-bit integer in the interval [0, p_n - 1]
inline __attribute__((always_inline)) uint64_t Eidos_rng_interval_uint64(EidosRNG_64_bit &rng_64, uint64_t p_n)
{
	// The gsl_rng_uniform_int() function is very slow, so this is a customized version that should be faster.
	// Basically it is faster because (1) the range of the generator is hard-coded, (2) the range check is done
	// only on #if DEBUG, and (3) it calls the generator directly, not through the GSL's pointer; otherwise the
	// logic is the same.
	uint64_t scale = UINT64_MAX / p_n;
	uint64_t k;
	
#if DEBUG
	if ((p_n > INT64_MAX) || (p_n <= 0)) 
	{
		GSL_ERROR_VAL("invalid n, either 0 or exceeds maximum value of generator", GSL_EINVAL, 0);
	}
#endif
	
	do
	{
		k = rng_64() / scale;
	}
	while (k >= p_n);
	
	return k;
}

// generates a random unsigned 64-bit integer in the interval [0, p_n - 1] using a fast but slightly biased
// algorithm; this should only be used for p_n << UINT32_MAX so that the bias is undetectable
inline __attribute__((always_inline)) uint64_t Eidos_rng_interval_uint64_FAST(EidosRNG_64_bit &rng_64, uint64_t p_n)
{
	// OK, so.  The GSL's uniform int method, which we replicate in Eidos_rng_interval_uint64(), makes sure
	// that the probability of each integer is exactly equal by figuring out a scaling, and then looping on
	// generated draws, with that scaling applied, until it gets one that is in range.  Here we skip that extra
	// work and just use modulo.  This technically means our draws will be biased toward the low end, unless
	// p_n is an exact divisor of UINT64_MAX, I guess; but UINT64_MAX is so vastly large compared to the uses
	// we will put this generator to that the bias should be utterly undetectable.  We are not drawing values
	// in anywhere near the full range of the generator.  BCH 12 May 2018
	return rng_64() % p_n;
}


// The gsl_ran_shuffle() function leans very heavily on gsl_rng_uniform_int(), which is very slow
// as mentioned above.  This is the same code as gsl_ran_shuffle(), but calls Eidos_rng_interval_uint32().
// It is also templated, to take advantage of std::swap(), which is faster than the GSL's generalized
// swap() function.  This uses the Fisher-Yates algorithm.  BCH 12/30/2022: I tried using a different
// algorithm called MergeShuffle (https://arxiv.org/abs/1508.03167), which is a parallelizable algorithm
// with code available on GitHub; the authors claim that it is faster than Fisher-Yates.  For me it was
// about twice as slow, in the sequential case, and with the speedup from parallelization it was still
// a little slower.  It looks like their observed speed was due to cache locality (less of a win now,
// as caches get ever bigger) and hand-tuned assembly code (not an option for us), and maybe also a
// custom fast RNG optimized for generating single random bits.  So, Fisher-Yates it is.
template <class T> inline void Eidos_ran_shuffle_uint32(EidosRNG_32_bit &rng_32, T *base, uint32_t n)
{
	for (uint32_t i = n - 1; i > 0; i--)
	{
		uint32_t j = Eidos_rng_interval_uint32(rng_32, i + 1);
		
		std::swap(base[i], base[j]);
	}
}

template <class T> inline void Eidos_ran_shuffle_uint64(EidosRNG_64_bit &rng_64, T *base, uint64_t n)
{
	for (uint64_t i = n - 1; i > 0; i--)
	{
		uint64_t j = Eidos_rng_interval_uint64(rng_64, i + 1);
		
		std::swap(base[i], base[j]);
	}
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

inline __attribute__((always_inline)) unsigned int Eidos_FastRandomPoisson(Eidos_RNG_State *rng_state, double p_mu)
{
	RNG_INIT_CHECK();
	
	// Defer to the GSL for large values of mu; see comments above.
	if (p_mu > 250)
		return gsl_ran_poisson(&rng_state->gsl_rng_, p_mu);
	
	unsigned int x = 0;
	double p = exp(-p_mu);
	double s = p;
	double u = Eidos_rng_uniform_doubleCO(rng_state->pcg64_rng_);
	
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
inline __attribute__((always_inline)) unsigned int Eidos_FastRandomPoisson(Eidos_RNG_State *rng_state, double p_mu, double p_exp_neg_mu)
{
	RNG_INIT_CHECK();
	
	// Defer to the GSL for large values of mu; see comments above.
	if (p_mu > 250)
		return gsl_ran_poisson(&rng_state->gsl_rng_, p_mu);
	
	// Test consistency; normally this is commented out
	//if (p_exp_neg_mu != exp(-p_mu))
	//	EIDOS_TERMINATION << "ERROR (Eidos_FastRandomPoisson): p_exp_neg_mu incorrect." << EidosTerminate(nullptr);
	
	unsigned int x = 0;
	double p = p_exp_neg_mu;
	double s = p;
	double u = Eidos_rng_uniform_doubleCO(rng_state->pcg64_rng_);
	
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
// The GSL declares gsl_rng* parameters as const, which seems wrong and confuses cppcheck...
// cppcheck-suppress constParameterPointer
inline __attribute__((always_inline)) unsigned int Eidos_FastRandomPoisson_NONZERO(Eidos_RNG_State *rng_state, double p_mu, double p_exp_neg_mu)
{
	RNG_INIT_CHECK();
	
	// Defer to the GSL for large values of mu; see comments above.
	if (p_mu > 250)
	{
		unsigned int result;
		
		do
		{
			result = gsl_ran_poisson(&rng_state->gsl_rng_, p_mu);
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
	double u = Eidos_rng_uniform_doubleOO(rng_state->pcg64_rng_);	// exclude 0.0 so u != s after rescaling
	
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


#pragma mark -
#pragma mark Random coin-flips
#pragma mark -

// get a random bool from a random number generator
//inline bool Eidos_RandomBool(gsl_rng *p_r) { return (bool)(taus_get(p_r->state) & 0x01); }

// optimization of this is possible assuming each bit returned by the RNG is independent and usable as a random boolean.
// the independence of all 64 bits seems to be a solid assumption for the MT64 generator, as far as I can tell.
inline __attribute__((always_inline)) bool Eidos_RandomBool(Eidos_RNG_State *rng_state)
{
	RNG_INIT_CHECK();
	
#if 0
	// This method gets one uint64_t at a time, and uses all 64 of its bits as random bools.  This is
	// aesthetically nice, but does have overhead for the branch, and for reading and writing the state.
	bool retval;
	
	if (rng_state->random_bool_bit_counter_ > 0)
	{
		rng_state->random_bool_bit_counter_--;
		rng_state->random_bool_bit_buffer_ >>= 1;
		retval = rng_state->random_bool_bit_buffer_ & 0x01;
	}
	else
	{
		rng_state->random_bool_bit_buffer_ = rng_state->pcg64_rng_();	// pcg64 provides 64 independent bits
		rng_state->random_bool_bit_counter_ = 63;						// 64 good bits originally, and we're about to use one
		
		retval = rng_state->random_bool_bit_buffer_ & 0x01;
	}
	
	return retval;
#elif 1
	// BCH 11/1/2025: This is an optimized version of the version above.  Rather than shifting the bit buffer
	// down by 1 and writing it out again, it shifts down by the count each time, so it doesn't need to write it.
	int bit_counter = rng_state->random_bool_bit_counter_;
	
	if (bit_counter > 0)
	{
		rng_state->random_bool_bit_counter_ = --bit_counter;
		
		return (rng_state->random_bool_bit_buffer_ >> bit_counter) & 0x01;
	}
	else
	{
		uint64_t next_uint64 = rng_state->pcg64_rng_();					// pcg64 provides 64 independent bits
		
		rng_state->random_bool_bit_buffer_ = next_uint64;
		rng_state->random_bool_bit_counter_ = 63;						// 64 good bits originally, and we're about to use one
		
		return (next_uint64 >> 63);
	}
#else
	// This version simply generates a new uint32 each time and uses its lowest-order bit; I was curious to
	// see whether, with the pcg32_fast generator, this might actually be faster (no branches), but it isn't.
	uint32_t next_uint32 = rng_state->pcg32_rng_();					// pcg32 provides a good low-order bit
	
	return (next_uint32 & 0x01);
#endif
}


#endif /* defined(__Eidos__eidos_rng__) */




































































