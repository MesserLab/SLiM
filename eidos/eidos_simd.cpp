//
//  eidos_simd.cpp
//  Eidos
//
//  Created by Andrew Kern on 5/21/2026.
//  Copyright (c) 2024-2025 Benjamin C. Haller.  All rights reserved.
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

 SIMD runtime dispatcher.  This translation unit is compiled at the plain
 baseline ABI (no instruction-set flags); it owns the public Eidos_SIMD
 function pointers and selects a tier for them at startup.  See eidos_simd.h.

 */

#include "eidos_simd.h"

#include <cstring>


// The public kernel pointers.  They are statically initialized to the scalar
// tier so that a call is well-defined even if it somehow happens before
// Eidos_SIMD_Init() runs; the address of a function is a constant expression,
// so there is no static-initialization-order dependency here.
namespace Eidos_SIMD {
#define X(ret, name, params) ret (*name) params = &Eidos_SIMD_scalar::name;
EIDOS_SIMD_FUNCTION_TABLE
#undef X
}


enum class Eidos_SIMD_Tier { kScalar, kSSE42, kAVX2_FMA, kNEON };

static Eidos_SIMD_Tier sActiveTier = Eidos_SIMD_Tier::kScalar;


bool Eidos_SIMD_SelectTier(const char *tier_name)
{
	// The scalar tier is built on every platform and always available.
	if (std::strcmp(tier_name, "scalar") == 0)
	{
		Eidos_SIMD_Fill_scalar();
		sActiveTier = Eidos_SIMD_Tier::kScalar;
		return true;
	}

#if EIDOS_SIMD_DISPATCH_X86
	// __builtin_cpu_supports() reads CPUID; it is available on GCC and Clang
	// for x86 and works regardless of the flags this file was compiled with.
	// AVX2 and FMA shipped together (Haswell), but we require both explicitly
	// since the AVX2 tier and SLEEF both use FMA instructions.
	if (std::strcmp(tier_name, "AVX2+FMA") == 0)
	{
		if (!(__builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma")))
			return false;
		Eidos_SIMD_Fill_avx2();
		sActiveTier = Eidos_SIMD_Tier::kAVX2_FMA;
		return true;
	}
	if (std::strcmp(tier_name, "SSE4.2") == 0)
	{
		if (!__builtin_cpu_supports("sse4.2"))
			return false;
		Eidos_SIMD_Fill_sse42();
		sActiveTier = Eidos_SIMD_Tier::kSSE42;
		return true;
	}
#endif

#if EIDOS_SIMD_DISPATCH_ARM
	// NEON is baseline on every ARM64 CPU, so it is always available here.
	if (std::strcmp(tier_name, "NEON") == 0)
	{
		Eidos_SIMD_Fill_neon();
		sActiveTier = Eidos_SIMD_Tier::kNEON;
		return true;
	}
#endif

	return false;
}

void Eidos_SIMD_Init(void)
{
	// Install the fastest tier the CPU supports. This is idempotent: calling it
	// again re-runs detection and re-installs the same tier, which is how the
	// SIMD self-tests restore normal dispatch after cycling through every tier.
#if EIDOS_SIMD_DISPATCH_X86
	if (Eidos_SIMD_SelectTier("AVX2+FMA"))
		return;
	if (Eidos_SIMD_SelectTier("SSE4.2"))
		return;
#endif
#if EIDOS_SIMD_DISPATCH_ARM
	if (Eidos_SIMD_SelectTier("NEON"))
		return;
#endif
	// Fallback for pre-AVX2/pre-SSE4.2 x86, unknown architectures, MSVC, and
	// USE_SIMD=OFF builds: the scalar tier, which runs on any CPU.
	Eidos_SIMD_SelectTier("scalar");
}

const char *Eidos_SIMD_ActiveTierName(void)
{
	switch (sActiveTier)
	{
		case Eidos_SIMD_Tier::kAVX2_FMA:	return "AVX2+FMA";
		case Eidos_SIMD_Tier::kSSE42:		return "SSE4.2";
		case Eidos_SIMD_Tier::kNEON:		return "NEON";
		case Eidos_SIMD_Tier::kScalar:		return "scalar";
	}
	return "scalar";
}

bool Eidos_SIMD_SLEEFActive(void)
{
	// SLEEF transcendentals are wired up only for the AVX2+FMA and NEON tiers.
	return (sActiveTier == Eidos_SIMD_Tier::kAVX2_FMA) || (sActiveTier == Eidos_SIMD_Tier::kNEON);
}
