//
//  eidos_rng.cpp
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


#include "eidos_rng.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
	// --- These are the required includes for the new Windows RNG code ---
	#include <windows.h>  // Main header for Windows data types like PBYTE
	#include <bcrypt.h>   // Header for the Cryptography API where BCryptGenRandom is declared
#else
	// --- These are the other headers, now only used for non-Windows systems ---
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/time.h>
#endif


// GSL-compatible wrapper for the PCG64 generator
static void Eidos_GSL_RNG_PCG64_set(void *state, unsigned long int seed)
{
#pragma unused(state, seed)
	// BCH 11/1/2025: This should never be called, because gsl_rng_set() should never be called.  The reason
	// is that the pcg64_fast generator is a bit fussy about seeds; we should always seed it through the
	// procedure followed in _Eidos_SetOneRNGSeed().  For that reason, we just skip seeding here.
#if 0
	EidosRNG_64_bit *rng_64 = static_cast<EidosRNG_64_bit *>(state);
	
	rng_64->seed(seed);
#endif
}

static unsigned long int Eidos_GSL_RNG_PCG64_get(void *state)
{
	EidosRNG_64_bit *rng_64 = static_cast<EidosRNG_64_bit *>(state);
	
	return (*rng_64)();
}

static double Eidos_GSL_RNG_PCG64_get_double(void *state)
{
	EidosRNG_64_bit *rng_64 = static_cast<EidosRNG_64_bit *>(state);
	
	// generates a random double in [0,1) -- including 0 but NOT 1
	// this is a copy of Eidos_rng_uniform_doubleCO()
	return ((*rng_64)() >> 11) * (1.0/9007199254740992.0);
}

static const gsl_rng_type gEidos_GSL_RNG_PCG64 = {
	"PCG64",
	UINT_MAX,
	0,
	sizeof(EidosRNG_64_bit),
	Eidos_GSL_RNG_PCG64_set,
	Eidos_GSL_RNG_PCG64_get,
	Eidos_GSL_RNG_PCG64_get_double
};


bool gEidos_RNG_Initialized = false;

#ifndef _OPENMP
Eidos_RNG_State gEidos_RNG_SINGLE;
#else
std::vector<Eidos_RNG_State *> gEidos_RNG_PERTHREAD;
#endif


static unsigned long int _Eidos_GenerateRNGSeed(void)
{
	unsigned long int seed;
	
#ifdef _WIN32
	// On Windows, use the Cryptography API: Next Generation (CNG) to get a
	// cryptographically secure random number for use as a seed.

	// Ensure this section is thread-safe. If multiple threads request a seed
	// simultaneously, they must take turns to avoid corrupting system resources.
#pragma omp critical (Eidos_GenerateRNGSeed)
	{
		// Ask Windows to fill our seed variable with random bytes.
		NTSTATUS status = BCryptGenRandom(NULL, (PBYTE)&seed, sizeof(seed), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

		// A failure here indicates a system-level problem with Windows. 
		if (!BCRYPT_SUCCESS(status))
		{
			fprintf(stderr, "ERROR: Failed to generate a random seed using BCryptGenRandom. Status: 0x%X\n", (unsigned int)status);
			exit(EXIT_FAILURE);
		}	
	}
#else
	// On other platforms, we now use /dev/urandom as a source of seeds, which is more reliably random.
	// Thanks to https://security.stackexchange.com/a/184211/288172 for the basis of this code.
	// I chose urandom rather than random to avoid stalls if the random pool's entropy is low;
	// semi-pseudorandom seeds should be good enough for our purposes here.
	
	// Ensure this section is thread-safe. If multiple threads request a seed
	// simultaneously, they must take turns to avoid corrupting system resources.
#pragma omp critical (Eidos_GenerateRNGSeed)
	{
		int fd = open("/dev/urandom", O_RDONLY);
		(void)(read(fd, &seed, sizeof(seed)) + 1);	// ignore the result without a warning, ugh; see https://stackoverflow.com/a/13999461
		close(fd);
	}
#endif
	
	// Our new pcg32_fast and pcg64_fast generators have a quirk: they ignore the lowest two bits of
	// the seed, and thus in _Eidos_SetOneRNGSeed() we shift the given seed left two places.  We want
	// to make room for that without overflow, so we shift right here.  I've chosen to shift right
	// by three places here, in fact, so that we have some extra headroom for the user to increment
	// the generated seed a couple of times without risking overflow.
	seed >>= 3;
	
	return seed;
}

unsigned long int Eidos_GenerateRNGSeed(void)
{
	// We impose an extra restriction that _Eidos_GenerateRNGSeed() does not worry about: we require
	// that the seed be greater than zero, as an int64_t.  We achieve that by just forcing it to be
	// true; if _Eidos_GenerateRNGSeed() is generating cryptographically secure seeds, that ought
	// to be harmless.  We do this so that the seed reported to the user always matches the seed
	// value generated (otherwise a discrepancy is visible in SLiMgui).
	// BCH 11/1/2025: Avoiding zero was needed for the old taus2 RNG in the GSL.  This is not really
	// needed any more, I think; but maybe avoiding zero is good anyway?
	int64_t seed_i64;
	
	do
	{
		unsigned long int seed = _Eidos_GenerateRNGSeed();
		unsigned long int clipped_seed = (seed & INT64_MAX);	// shave off the top bit
		
		seed_i64 = (int64_t)clipped_seed;
	} while (seed_i64 == 0);
	
	return (unsigned long int)seed_i64;
}

void _Eidos_InitializeOneRNG(Eidos_RNG_State &r)
{
	// Note that this is now called from each thread, when running parallel
	r.rng_last_seed_ = 0;
	
	r.pcg32_rng_.seed(0);
	r.pcg64_rng_.seed(0);
	
	// we do not call gsl_rng_alloc(), because our gsl_rng instance is inline; the GSL unfortunately
	// does not cater to this possibility, so we've got a bit of copied init code here from the GSL
	r.gsl_rng_.type = &gEidos_GSL_RNG_PCG64;
	r.gsl_rng_.state = &r.pcg64_rng_;	// the "state" pointer points to our 64-bit PCG generator
	//gsl_rng_set(&r.gsl_rng_, 0);      // the generator was already seeded above through pcg64_rng_
	
	r.random_bool_bit_counter_ = 0;
	r.random_bool_bit_buffer_ = 0;
}

void Eidos_InitializeRNG(void)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Eidos_InitializeRNG(): RNG change");
	
	// Allocate the RNG if needed
	if (gEidos_RNG_Initialized)
		EIDOS_TERMINATION << "ERROR (Eidos_InitializeRNG): (internal error) the Eidos random number generator has already been allocated." << EidosTerminate(nullptr);
	
#ifndef _OPENMP
	//std::cout << "***** Initializing one single-threaded RNG" << std::endl;
	
	_Eidos_InitializeOneRNG(gEidos_RNG_SINGLE);
#else
	//std::cout << "***** Initializing " << gEidosMaxThreads << " independent RNGs" << std::endl;
	
	gEidos_RNG_PERTHREAD.resize(gEidosMaxThreads);
	
	// Check that each RNG was initialized by a different thread, as intended below;
	// this is not required, but it improves memory locality throughout the run
	bool threadObserved[gEidosMaxThreads];
	
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, threadObserved) num_threads(gEidosMaxThreads)
	{
		// Each thread allocates and initializes its own Eidos_RNG_State, for "first touch" optimization
		int threadnum = omp_get_thread_num();
		Eidos_RNG_State *rng_state = (Eidos_RNG_State *)calloc(1, sizeof(Eidos_RNG_State));
		_Eidos_InitializeOneRNG(*rng_state);
		gEidos_RNG_PERTHREAD[threadnum] = rng_state;
		threadObserved[threadnum] = true;
	}	// end omp parallel
	
	for (int threadnum = 0; threadnum < gEidosMaxThreads; ++threadnum)
		if (!threadObserved[threadnum])
			std::cerr << "WARNING: parallel RNGs were not correctly initialized on their corresponding threads; this may cause slower random number generation." << std::endl;
#endif	// end _OPENMP
	
	gEidos_RNG_Initialized = true;
}

void _Eidos_FreeOneRNG(Eidos_RNG_State &r)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("_Eidos_FreeOneRNG(): RNG change");
	
	r.gsl_rng_.type = NULL;		// not owned
	r.gsl_rng_.state = NULL;	// not owned
	
	r.random_bool_bit_buffer_ = 0;
	r.random_bool_bit_counter_ = 0;
}

void Eidos_FreeRNG(void)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Eidos_FreeRNG(): RNG change");
		
	if (!gEidos_RNG_Initialized)
		EIDOS_TERMINATION << "ERROR (Eidos_FreeRNG): (internal error) the Eidos random number generator has not been allocated." << EidosTerminate(nullptr);
	
#ifndef _OPENMP
	//std::cout << "***** Freeing one single-threaded RNG" << std::endl;
	
	_Eidos_FreeOneRNG(gEidos_RNG_SINGLE);
#else
	//std::cout << "***** Freeing " << gEidosMaxThreads << " independent RNGs" << std::endl;
	
	for (int threadIndex = 0; threadIndex < gEidosMaxThreads; ++threadIndex)
	{
		Eidos_RNG_State *rng_state = gEidos_RNG_PERTHREAD[threadIndex];
		_Eidos_FreeOneRNG(*rng_state);
		gEidos_RNG_PERTHREAD[threadIndex] = nullptr;
		free(rng_state);
	}
	
	gEidos_RNG_PERTHREAD.resize(0);
#endif
	
	gEidos_RNG_Initialized = false;
}

void _Eidos_SetOneRNGSeed(Eidos_RNG_State &r, unsigned long int p_seed)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("_Eidos_SetOneRNGSeed(): RNG change");
	
	//std::cout << "***** Setting seed " << p_seed << " on RNG" << std::endl;
	
	// pcg32_rng_ and pcg64_rng_ need the seed to be shifted left by two; the lowest two bits don't matter
	// see https://github.com/imneme/pcg-cpp/issues/79 for details on this, which seems to be a bug
	r.pcg32_rng_.seed(p_seed << 2);
	r.pcg64_rng_.seed(p_seed << 2);
	
	// we seem to need to re-point gsl_rng_ to pcg64_rng_; I don't really understand quite why...
	r.gsl_rng_.state = &r.pcg64_rng_;
	//gsl_rng_set(&r.gsl_rng_, p_seed);      // the generator was already seeded above through pcg64_rng_
	
	// remember the original user-supplied seed as part of the RNG state
	r.rng_last_seed_ = p_seed;
	
	// The random bit buffer state needs to be zeroed out, too; it is part of our RNG state
	r.random_bool_bit_counter_ = 0;
	r.random_bool_bit_buffer_ = 0;
}

void Eidos_SetRNGSeed(unsigned long int p_seed)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Eidos_SetRNGSeed(): RNG change");
	
	if (!gEidos_RNG_Initialized)
		EIDOS_TERMINATION << "ERROR (Eidos_SetRNGSeed): (internal error) the Eidos random number generator has not been allocated." << EidosTerminate(nullptr);
	
#ifndef _OPENMP
	_Eidos_SetOneRNGSeed(gEidos_RNG_SINGLE, p_seed);
#else
	// Each thread's RNG gets a different seed.  We use the user-supplied seed for thread 0, so that non-parallel
	// code continues to reproduce the same random number sequence.  For other threads, we use system-generated seed
	// values.  This is non-reproducible, but parallel code involving the RNG is non-reproducible anyway.
	for (int threadIndex = 0; threadIndex < gEidosMaxThreads; ++threadIndex)
	{
		unsigned long int thread_seed = (threadIndex == 0 ? p_seed : Eidos_GenerateRNGSeed());
		
		_Eidos_SetOneRNGSeed(*gEidos_RNG_PERTHREAD[threadIndex], thread_seed);
	}
#endif
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





































