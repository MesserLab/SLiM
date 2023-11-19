//
//  eidos_rng.cpp
//  Eidos
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>


bool gEidos_RNG_Initialized = false;

#ifndef _OPENMP
Eidos_RNG_State gEidos_RNG_SINGLE;
#else
std::vector<Eidos_RNG_State *> gEidos_RNG_PERTHREAD;
#endif


static unsigned long int _Eidos_GenerateRNGSeed(void)
{
#ifdef _WIN32
	// on Windows, we continue to hash together the PID and the time; I think there is a
	// Windows API for cryptographic random numbers, though.  FIXME
	static long int hereCounter = 0;
	long long milliseconds;
	
#pragma omp critical (Eidos_GenerateRNGSeed)
	{
		pid_t pid = getpid();
		struct timeval te;
		
		gettimeofday(&te, NULL); // get current time
		
		milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;	// calculate milliseconds
		milliseconds += (pid * (long long)10000000);		// try to make the pid matter a lot, to separate runs made on different cores close in time
		milliseconds += (hereCounter * (long long)100000);	// make successive calls to this function be widely separated in their seeds
		hereCounter++;
	}
	
	return (unsigned long int)milliseconds;
#else
	// on other platforms, we now use /dev/urandom as a source of seeds, which is more reliably random
	// thanks to https://security.stackexchange.com/a/184211/288172 for the basis of this code
	// chose urandom rather than random to avoid stalls if the random pool's entropy is low;
	// semi-pseudorandom seeds should be good enough for our purposes here
	unsigned long int seed;
	
#pragma omp critical (Eidos_GenerateRNGSeed)
	{
		int fd = open("/dev/urandom", O_RDONLY);
		(void)(read(fd, &seed, sizeof(seed)) + 1);	// ignore the result without a warning, ugh; see https://stackoverflow.com/a/13999461
		close(fd);
	}
	
	return seed;
#endif
}

unsigned long int Eidos_GenerateRNGSeed(void)
{
	// We impose an extra restriction that _Eidos_GenerateRNGSeed() does not worry about: we require
	// that the seed be greater than zero, as an int64_t.  We achieve that by just forcing it to be
	// true; if _Eidos_GenerateRNGSeed() is generating cryptographically secure seeds, that ought
	// to be harmless.  We do this so that the seed reported to the user always matches the seed
	// value generated (otherwise a discrepancy is visible in SLiMgui).
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
	
	r.gsl_rng_ = gsl_rng_alloc(gsl_rng_taus2);	// the assumption of taus2 is hard-coded in eidos_rng.h
	
	r.mt_rng_.mt_ = (uint64_t *)malloc(Eidos_MT64_NN * sizeof(uint64_t));
	r.mt_rng_.mti_ = Eidos_MT64_NN + 1;				// mti==NN+1 means mt[NN] is not initialized
	
	r.random_bool_bit_counter_ = 0;
	r.random_bool_bit_buffer_ = 0;
	
	if (!r.gsl_rng_ || !r.mt_rng_.mt_)
		EIDOS_TERMINATION << "ERROR (_Eidos_InitializeOneRNG): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
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
	
	if (r.gsl_rng_)
	{
		gsl_rng_free(r.gsl_rng_);
		r.gsl_rng_ = NULL;
	}
	
	if (r.mt_rng_.mt_)
	{
		free(r.mt_rng_.mt_);
		r.mt_rng_.mt_ = NULL;
	}
	r.mt_rng_.mti_ = 0;
	
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
	
	// BCH 12 Sept. 2016: it turns out that gsl_rng_taus2 produces exactly the same sequence for seeds 0 and 1.  This is obviously
	// undesirable; people will often do a set of runs with sequential seeds starting at 0 and counting up, and they will get
	// identical runs for 0 and 1.  There is no way to re-map the seed space to get rid of the problem altogether; all we can do
	// is shift it to a place where it is unlikely to cause a problem.  So that's what we do.
	if ((p_seed > 0) && (p_seed < 10000000000000000000UL))
		gsl_rng_set(r.gsl_rng_, p_seed + 1);	// map 1 -> 2, 2-> 3, 3-> 4, etc.
	else
		gsl_rng_set(r.gsl_rng_, p_seed);		// 0 stays 0
	
	// BCH 13 May 2018: set the seed on the MT64 generator as well; we keep them synchronized in their seeding
	Eidos_MT64_init_genrand64(&r.mt_rng_, p_seed);
	
	//std::cout << "***** Setting seed " << p_seed << " on RNG" << std::endl;
	
	// remember the seed as part of the RNG state
	
	// BCH 12 Sept. 2016: we want to return the user the same seed they requested, if they call getSeed(), so we save the requested
	// seed, not the seed shifted by one that is actually passed to the GSL above.
	r.rng_last_seed_ = p_seed;
	
	// These need to be zeroed out, too; they are part of our RNG state
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


#pragma mark -
#pragma mark 64-bit MT
#pragma mark -

// This is a 64-bit Mersenne Twister implementation.  The code below is used in accordance with its license,
// reproduced in eidos_rng.h.  See eidos_rng.h for further comments on this code; most of the code is there.

/* initializes mt[NN] with a seed */
void Eidos_MT64_init_genrand64(Eidos_MT_State *r, uint64_t seed)
{
	r->mt_[0] = seed;
	for (r->mti_ = 1; r->mti_ < Eidos_MT64_NN; r->mti_++) 
		r->mt_[r->mti_] =  (6364136223846793005ULL * (r->mt_[r->mti_ - 1] ^ (r->mt_[r->mti_ - 1] >> 62)) + r->mti_);
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
void Eidos_MT64_init_by_array64(Eidos_MT_State *r, uint64_t init_key[], uint64_t key_length)
{
	uint64_t i, j, k;
	Eidos_MT64_init_genrand64(r, 19650218ULL);
	i=1; j=0;
	k = (Eidos_MT64_NN>key_length ? Eidos_MT64_NN : key_length);
	for (; k; k--) {
		r->mt_[i] = (r->mt_[i] ^ ((r->mt_[i-1] ^ (r->mt_[i-1] >> 62)) * 3935559000370003845ULL))
		+ init_key[j] + j; /* non linear */
		i++; j++;
		if (i>=Eidos_MT64_NN) { r->mt_[0] = r->mt_[Eidos_MT64_NN-1]; i=1; }
		if (j>=key_length) j=0;
	}
	for (k=Eidos_MT64_NN-1; k; k--) {
		r->mt_[i] = (r->mt_[i] ^ ((r->mt_[i-1] ^ (r->mt_[i-1] >> 62)) * 2862933555777941757ULL))
		- i; /* non linear */
		i++;
		if (i>=Eidos_MT64_NN) { r->mt_[0] = r->mt_[Eidos_MT64_NN-1]; i=1; }
	}
	
	r->mt_[0] = 1ULL << 63; /* MSB is 1; assuring non-zero initial array */ 
}

/* BCH: fill the next Eidos_MT64_NN words; used internally by genrand64_int64() */
void _Eidos_MT64_fill(Eidos_MT_State *r)
{
	/* generate NN words at one time */
	/* if init_genrand64() has not been called, */
	/* a default initial seed is used     */
	int i;
	static uint64_t mag01[2]={0ULL, Eidos_MT64_MATRIX_A};
	uint64_t x;
	
	// In the original code, this would fall back to some default seed value, but we
	// don't want to allow the RNG to be used without being seeded first.  BCH 5/13/2018
	if (r->mti_ == Eidos_MT64_NN+1) 
		abort(); 
	
	for (i=0;i<Eidos_MT64_NN-Eidos_MT64_MM;i++) {
		x = (r->mt_[i]&Eidos_MT64_UM)|(r->mt_[i+1]&Eidos_MT64_LM);
		r->mt_[i] = r->mt_[i+Eidos_MT64_MM] ^ (x>>1) ^ mag01[(int)(x&1ULL)];
	}
	for (;i<Eidos_MT64_NN-1;i++) {
		x = (r->mt_[i]&Eidos_MT64_UM)|(r->mt_[i+1]&Eidos_MT64_LM);
		r->mt_[i] = r->mt_[i+(Eidos_MT64_MM-Eidos_MT64_NN)] ^ (x>>1) ^ mag01[(int)(x&1ULL)];
	}
	x = (r->mt_[Eidos_MT64_NN-1]&Eidos_MT64_UM)|(r->mt_[0]&Eidos_MT64_LM);
	r->mt_[Eidos_MT64_NN-1] = r->mt_[Eidos_MT64_MM-1] ^ (x>>1) ^ mag01[(int)(x&1ULL)];
	
	r->mti_ = 0;
}
































































