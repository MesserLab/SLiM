//
//  eidos_rng.h
//  Eidos
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
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
 
 Eidos uses a globally shared random number generator called gEidos_RNG.  This file defines that global and relevant helper functions.
 
 */

#ifndef __Eidos__eidos_rng__
#define __Eidos__eidos_rng__


// We have our own private copy of (parts of) the GSL library, so that we don't have link dependencies.
// See the _README file in the gsl directory for information on the local copy of the GSL included in this project.
#include "gsl_rng.h"
#include "gsl_randist.h"

#include <stdint.h>
#include <cmath>
#include "eidos_globals.h"


// This cruft belongs to the 64-bit Mersenne Twister code below; it is up here because we need it to define the global RNG
// struct and state below.  See below for all the rest of the 64-bit MT code, including copyrights and credits and license.
uint64_t Eidos_MT64_genrand64_int64(void);

// OK, so.  This header defines the Eidos random number generator, which is now a bit of a weird hybrid.  We need to use
// the GSL's RNG for most purposes, because we want to use its random distributions and so forth.  However, the taus2
// RNG that we use from the GSL only generates samples in [0, UINT32_MAX-1], and for some applications we need samples
// in [0, UINT64_MAX-1].  For that purpose, we also have a 64-bit Mersenne Twister RNG.  We keep the taus2 and MT64
// generators synchronized, in the sense that we always seed them simultaneously with the same seed value.  As long as
// the user makes the same draws with the same calls, the fact that there are two generators under the hood shouldn't
// matter.  This struct defines all of the variables associated with both RNGs; this is the complete Eidos RNG state.
typedef struct
{
	unsigned long int rng_last_seed_;		// unsigned long int is the type used for seeds in the GSL
	
	// GSL taus2 generator
	gsl_rng *gsl_rng_;
	
	// MT64 generator; see below
	uint64_t *mt_;							// buffer of Eidos_MT64_NN uint64_t
	int mti_;
	
	// random coin-flip generator; based on the MT64 generator now
	int random_bool_bit_counter_;
	uint64_t random_bool_bit_buffer_;
} Eidos_RNG_State;


// This is the globally shared random number generator.  Note that the globals for random bit generation below are also
// considered to be part of the RNG state; if the Context plays games with swapping different RNGs in and out, those
// globals need to get swapped as well.  Likewise for the last seed value; this is part of the RNG state in Eidos.
// The 64-bit Mersenne Twister is also part of the overall global RNG state.
extern Eidos_RNG_State gEidos_RNG;

// Calls to the GSL should use this macro to avoid hard-coding the internals of Eidos_RNG_State
#define EIDOS_GSL_RNG	(gEidos_RNG.gsl_rng_)


// generate a new random number seed from the PID and clock time
unsigned long int Eidos_GenerateSeedFromPIDAndTime(void);

// set up the random number generator with a given seed
void Eidos_InitializeRNG(void);
void Eidos_FreeRNG(Eidos_RNG_State &p_rng);
void Eidos_SetRNGSeed(unsigned long int p_seed);


// This code is copied and modified from taus.c in the GSL library because we want to be able to inline taus_get().
// Random number generation can be a major bottleneck in many SLiM models, so I think this is worth the grossness.
typedef struct
{
	unsigned long int s1, s2, s3;
}
taus_state_t;

inline __attribute__((always_inline)) unsigned long
taus_get_inline (void *vstate)
{
	taus_state_t *state = (taus_state_t *) vstate;
	
#define TAUS_MASK 0xffffffffUL
#define TAUSWORTHE(s,a,b,c,d) (((s &c) <<d) &TAUS_MASK) ^ ((((s <<a) &TAUS_MASK)^s) >>b)
	
	state->s1 = TAUSWORTHE (state->s1, 13, 19, 4294967294UL, 12);
	state->s2 = TAUSWORTHE (state->s2, 2, 25, 4294967288UL, 4);
	state->s3 = TAUSWORTHE (state->s3, 3, 11, 4294967280UL, 17);
	
	return (state->s1 ^ state->s2 ^ state->s3);
}

#undef TAUS_MASK
#undef TAUSWORTHE


// The gsl_rng_uniform() function is a bit slow because of the indirection it goes through to get the
// function pointer, so this is a customized version that should be faster.  Basically it just hard-codes
// taus_get(); otherwise its logic is the same.  The taus_get_double() function called by gsl_rng_uniform()
// has the advantage of inlining the taus_get() function, but on the other hand, Eidos_rng_uniform() is
// itself inline, which gsl_rng_uniform()'s call to taus_get_double() cannot be, so that should be a wash.
inline __attribute__((always_inline)) double Eidos_rng_uniform(gsl_rng *p_r)
{
	return taus_get_inline(p_r->state) / 4294967296.0;
}

// Basically ditto; faster than gsl_rng_uniform_pos() by avoiding indirection.
inline __attribute__((always_inline)) double Eidos_rng_uniform_pos(const gsl_rng *p_r)
{
	double x;
	
	do
	{
		x = taus_get_inline(p_r->state) / 4294967296.0;
	}
	while (x == 0);
	
	return x;
}

// The gsl_rng_uniform_int() function is very slow, so this is a customized version that should be faster.
// Basically it is faster because (1) the range of the taus2 generator is hard-coded, (2) the range check
// is done only on #if DEBUG, (3) it uses uint32_t, and (4) it calls taus_get() directly; otherwise the
// logic is the same.
inline __attribute__((always_inline)) uint32_t Eidos_rng_uniform_int(gsl_rng *p_r, uint32_t p_n)
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
		k = ((uint32_t)(taus_get_inline(p_r->state))) / scale;		// taus_get is used by the taus2 RNG
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
		return gsl_ran_poisson(EIDOS_GSL_RNG, p_mu);
	
	unsigned int x = 0;
	double p = exp(-p_mu);
	double s = p;
	double u = Eidos_rng_uniform(EIDOS_GSL_RNG);
	
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
		return gsl_ran_poisson(EIDOS_GSL_RNG, p_mu);
	
	// Test consistency; normally this is commented out
	//if (p_exp_neg_mu != exp(-p_mu))
	//	EIDOS_TERMINATION << "ERROR (Eidos_FastRandomPoisson): p_exp_neg_mu incorrect." << EidosTerminate(nullptr);
	
	unsigned int x = 0;
	double p = p_exp_neg_mu;
	double s = p;
	double u = Eidos_rng_uniform(EIDOS_GSL_RNG);
	
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
			result = gsl_ran_poisson(EIDOS_GSL_RNG, p_mu);
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
	double u = Eidos_rng_uniform_pos(EIDOS_GSL_RNG);	// exclude 0.0 so u != s after rescaling
	
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
#pragma mark 64-bit MT
#pragma mark -

// This is a 64-bit Mersenne Twister implementation.  The code below is used in accordance with its license, reproduced below in full.
// This code, and associated header code, is from: http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/VERSIONS/C-LANG/mt19937-64.c
// Thanks to T. Nishimura and M. Matsumoto for this code.  I have modified the names of symbols, and added header declarations, and
// changed "unsigned long long" to uint64_t and "long long" to int64_t, and added a little semantic sugar on top to match the GSL in
// the areas we use this, and made some of the code inlined; the core algorithm is of course completely untouched.  BCH 12 May 2018.

/* 
 A C-program for MT19937-64 (2004/9/29 version).
 Coded by Takuji Nishimura and Makoto Matsumoto.
 
 This is a 64-bit version of Mersenne Twister pseudorandom number
 generator.
 
 Before using, initialize the state by using init_genrand64(seed)  
 or init_by_array64(init_key, key_length).
 
 Copyright (C) 2004, Makoto Matsumoto and Takuji Nishimura,
 All rights reserved.                          
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 
 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 3. The names of its contributors may not be used to endorse or promote 
 products derived from this software without specific prior written 
 permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 References:
 T. Nishimura, ``Tables of 64-bit Mersenne Twisters''
 ACM Transactions on Modeling and 
 Computer Simulation 10. (2000) 348--357.
 M. Matsumoto and T. Nishimura,
 ``Mersenne Twister: a 623-dimensionally equidistributed
 uniform pseudorandom number generator''
 ACM Transactions on Modeling and 
 Computer Simulation 8. (Jan. 1998) 3--30.
 
 Any feedback is very welcome.
 http://www.math.hiroshima-u.ac.jp/~m-mat/MT/emt.html
 email: m-mat @ math.sci.hiroshima-u.ac.jp (remove spaces)
 */

#define Eidos_MT64_NN 312
#define Eidos_MT64_MM 156
#define Eidos_MT64_MATRIX_A 0xB5026F5AA96619E9ULL
#define Eidos_MT64_UM 0xFFFFFFFF80000000ULL /* Most significant 33 bits */
#define Eidos_MT64_LM 0x7FFFFFFFULL /* Least significant 31 bits */

/* initializes mt[NN] with a seed */
void Eidos_MT64_init_genrand64(uint64_t seed);

/* initialize by an array with array-length */
void Eidos_MT64_init_by_array64(uint64_t init_key[], uint64_t key_length);

/* BCH: fill the next Eidos_MT64_NN words; used internally by genrand64_int64() */
void _Eidos_MT64_fill();

/* generates a random number on [0, 2^64-1]-interval */
inline __attribute__((always_inline)) uint64_t Eidos_MT64_genrand64_int64(void)
{
	/* generate NN words at one time */
	if (gEidos_RNG.mti_ >= Eidos_MT64_NN)
		_Eidos_MT64_fill();
	
	uint64_t x = gEidos_RNG.mt_[gEidos_RNG.mti_++];
	
	x ^= (x >> 29) & 0x5555555555555555ULL;
	x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
	x ^= (x << 37) & 0xFFF7EEE000000000ULL;
	x ^= (x >> 43);
	
	return x;
}

/* generates a random number on [0, 2^63-1]-interval */
inline __attribute__((always_inline)) int64_t Eidos_MT64_genrand64_int63(void)
{
	return (int64_t)(Eidos_MT64_genrand64_int64() >> 1);
}

/* generates a random number on [0,1]-real-interval */
inline __attribute__((always_inline)) double Eidos_MT64_genrand64_real1(void)
{
	return (Eidos_MT64_genrand64_int64() >> 11) * (1.0/9007199254740991.0);
}

/* generates a random number on [0,1)-real-interval */
inline __attribute__((always_inline)) double Eidos_MT64_genrand64_real2(void)
{
	return (Eidos_MT64_genrand64_int64() >> 11) * (1.0/9007199254740992.0);
}

/* generates a random number on (0,1)-real-interval */
inline __attribute__((always_inline)) double Eidos_MT64_genrand64_real3(void)
{
	return ((Eidos_MT64_genrand64_int64() >> 12) + 0.5) * (1.0/4503599627370496.0);
}

/* BCH: generates a random integer in [0, p_n - 1]; parallel to Eidos_rng_uniform_int() above */
inline __attribute__((always_inline)) uint64_t Eidos_rng_uniform_int_MT64(uint64_t p_n)
{
	// OK, so.  The GSL's uniform int method, whose logic we replicate in Eidos_rng_uniform_int(), makes sure
	// that the probability of each integer is exactly equal by figuring out a scaling, and then looping on
	// generated draws, with that scaling applied, until it gets one that is in range.  Here we skip that extra
	// work and just use modulo.  This technically means our draws will be biased toward the low end, unless
	// p_n is an exact divisor of UINT64_MAX, I guess; but UINT64_MAX is so vastly large compared to the uses
	// we will put this generator to that the bias should be utterly undetectable.  We are not drawing values
	// in anywhere near the full range of the generator; we just need a couple of orders of magnitude more
	// headroom than UINT32_MAX provides.  If we start to use this for a wider range of p_n (such as making it
	// available in the Eidos APIs), this decision would need to be revisited.  BCH 12 May 2018
	return Eidos_MT64_genrand64_int64() % p_n;
}


#pragma mark -
#pragma mark Random coin-flips
#pragma mark -

// get a random bool from a random number generator
//static inline bool Eidos_RandomBool(gsl_rng *p_r) { return (bool)(taus_get(p_r->state) & 0x01); }

// optimization of this is possible assuming each bit returned by the RNG is independent and usable as a random boolean.
// the independence of all 64 bits seems to be a solid assumption for the MT64 generator, as far as I can tell.
static inline __attribute__((always_inline)) bool Eidos_RandomBool()
{
	bool retval;
	
	if (gEidos_RNG.random_bool_bit_counter_ > 0)
	{
		gEidos_RNG.random_bool_bit_counter_--;
		gEidos_RNG.random_bool_bit_buffer_ >>= 1;
		retval = gEidos_RNG.random_bool_bit_buffer_ & 0x01;
	}
	else
	{
		gEidos_RNG.random_bool_bit_buffer_ = Eidos_MT64_genrand64_int64();	// MT64 provides 64 independent bits
		gEidos_RNG.random_bool_bit_counter_ = 63;				// 64 good bits originally, and we're about to use one
		
		retval = gEidos_RNG.random_bool_bit_buffer_ & 0x01;
	}
	
	return retval;
}


#endif /* defined(__Eidos__eidos_rng__) */




































































