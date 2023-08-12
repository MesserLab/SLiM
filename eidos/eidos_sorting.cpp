//
//  eidos_sorting.cpp
//  SLiM
//
//  Created by Ben Haller on 8/12/23.
//  Copyright (c) 2023 Philipp Messer.  All rights reserved.
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

#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <string>
#include <cmath>

#include "eidos_sorting.h"


// Generate the code for our sorting variants

#define SORTTYPE			int64_t
#define EIDOS_SORT_CUTOFF	EIDOS_OMPMIN_SORT_INT
#define EIDOS_SORT_THREADS	gEidos_OMP_threads_SORT_INT
#include "eidos_sorting.inc"

#define SORTTYPE			double
#define EIDOS_SORT_CUTOFF	EIDOS_OMPMIN_SORT_FLOAT
#define EIDOS_SORT_THREADS	gEidos_OMP_threads_SORT_FLOAT
#include "eidos_sorting.inc"

#define SORTTYPE			std::string
#define EIDOS_SORT_CUTOFF	EIDOS_OMPMIN_SORT_STRING
#define EIDOS_SORT_THREADS	gEidos_OMP_threads_SORT_STRING
#include "eidos_sorting.inc"





































