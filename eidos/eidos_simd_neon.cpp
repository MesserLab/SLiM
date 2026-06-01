//
//  eidos_simd_neon.cpp
//  Eidos
//
//  Created by Andrew Kern on 5/21/2026.
//  Copyright (c) 2026 Benjamin C. Haller.  All rights reserved.
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

 The NEON SIMD tier (ARM64 only).  NEON is part of the baseline ARM64
 architecture, so this translation unit needs no special compiler flags; it is
 simply compiled normally on ARM64.  On x86 architectures, when USE_SIMD=OFF,
 and on MSVC, this file compiles to nothing.

 */

#include "eidos_simd.h"

#if EIDOS_SIMD_DISPATCH_ARM

#define EIDOS_HAS_NEON 1
#define EIDOS_SIMD_IMPL_NAMESPACE Eidos_SIMD_neon
#include "eidos_simd_impl.h"

void Eidos_SIMD_Fill_neon(void)
{
#define X(ret, name, params) Eidos_SIMD::name = &Eidos_SIMD_neon::name;
EIDOS_SIMD_FUNCTION_TABLE
#undef X
}

#endif // EIDOS_SIMD_DISPATCH_ARM
