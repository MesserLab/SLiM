//
//  eidos_simd_avx2.cpp
//  Eidos
//
//  Created by Andrew Kern in 2026.
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

 The AVX2+FMA SIMD tier (x86_64 only).  This entire translation unit is built
 with -mavx2 -mfma by the build system (see CMakeLists.txt); no other
 translation unit gets those flags, so AVX2/FMA instructions (including those
 inside the SLEEF transcendental headers pulled in here) are confined to this
 file and reached only through the dispatch pointers after the CPU has been
 confirmed to support AVX2 and FMA.

 On non-x86 architectures, when USE_SIMD=OFF, and on MSVC, this file compiles
 to nothing.

 */

#include "eidos_simd.h"

#if EIDOS_SIMD_DISPATCH_X86

#define EIDOS_HAS_AVX2 1
#define EIDOS_HAS_FMA 1
#define EIDOS_SIMD_IMPL_NAMESPACE Eidos_SIMD_avx2
#include "eidos_simd_impl.h"

void Eidos_SIMD_Fill_avx2(void)
{
#define X(ret, name, params) Eidos_SIMD::name = &Eidos_SIMD_avx2::name;
EIDOS_SIMD_FUNCTION_TABLE
#undef X
}

#endif // EIDOS_SIMD_DISPATCH_X86
