//
//  eidos_simd_scalar.cpp
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

 The scalar SIMD tier: portable C++ with no instruction-set intrinsics, built
 on every platform and compiled at the plain baseline ABI.  This is the
 universal fallback and the static default for the Eidos_SIMD pointers.

 */

#include "eidos_simd.h"

// No EIDOS_HAS_* macro is defined, so eidos_simd_impl.h expands to the scalar
// code paths.
#define EIDOS_SIMD_IMPL_NAMESPACE Eidos_SIMD_scalar
#include "eidos_simd_impl.h"

void Eidos_SIMD_Fill_scalar(void)
{
#define X(ret, name, params) Eidos_SIMD::name = &Eidos_SIMD_scalar::name;
EIDOS_SIMD_FUNCTION_TABLE
#undef X
}
