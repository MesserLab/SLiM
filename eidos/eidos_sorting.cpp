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

#include "eidos_sorting.h"
#include "eidos_openmp.h"


// Here are some early explorations into parallelizing sorting.  The speedups
// here are not particularly impressive.  Parallel sorting is a very deep and
// complex rabbit hole to go down; see, e.g., Victor Duvanenko's work at
// https://github.com/DragonSpit/ParallelAlgorithms.  But those algorithms
// use TBB instead of OpenMP, and require C++17, so they're not easily usable.
// Wikipedia at https://en.wikipedia.org/wiki/Merge_sort#Parallel_merge_sort
// also has some very interesting commentary about parallelization of sorting.
// The code here is also very primitive - integer only, no templates, no
// client-suppliable comparator, etc. – so it would be hard to integrate into
// all the ways Eidos presently uses std::sort.  Improving this looks like a
// good project for somebody in CS.

// This parallel quicksort code is thanks to Ruud van der Pas, modified from
// https://www.openmp.org/wp-content/uploads/sc16-openmp-booth-tasking-ruud.pdf
#ifdef _OPENMP
static void _Eidos_ParallelQuicksort_I(int64_t *values, int64_t lo, int64_t hi)
{
	if (lo >= hi)
		return;
	
	if (hi - lo + 1 <= 1000) {
		// fall over to using std::sort when below a threshold interval size
		// the larger the threshold, the less time we spend thrashing tasks on small
		// intervals, which is good; but it also sets a limit on how many threads we
		// we bring to bear on relatively small sorts, which is bad; 1000 seems fine
		std::sort(values + lo, values + hi + 1);
	} else {
		// choose the middle of three pivots, in an attempt to avoid really bad pivots
		int64_t pivot1 = *(values + lo);
		int64_t pivot2 = *(values + hi);
		int64_t pivot3 = *(values + ((lo + hi) >> 1));
		int64_t pivot;
		
		if (pivot1 > pivot2)
		{
			if (pivot2 > pivot3)		pivot = pivot2;
			else if (pivot1 > pivot3)	pivot = pivot3;
			else						pivot = pivot1;
		}
		else
		{
			if (pivot1 > pivot3)		pivot = pivot1;
			else if (pivot2 > pivot3)	pivot = pivot3;
			else						pivot = pivot2;
		}
		
		// note that std::partition is not guaranteed to leave the pivot value in position
		// we do a second partition to exclude all duplicate pivot values, which seems to be one standard strategy
		// this works particularly well when duplicate values are very common; it helps avoid O(n^2) performance
		// note the partition is not parallelized; that is apparently a difficult problem for parallel quicksort
		int64_t *middle1 = std::partition(values + lo, values + hi + 1, [pivot](const int64_t& em) { return em < pivot; });
		int64_t *middle2 = std::partition(middle1, values + hi + 1, [pivot](const int64_t& em) { return !(pivot < em); });
		int64_t mid1 = middle1 - values;
		int64_t mid2 = middle2 - values;
		#pragma omp task default(none) firstprivate(values, lo, mid1)
		{ _Eidos_ParallelQuicksort_I(values, lo, mid1 - 1); }	// Left branch
		#pragma omp task default(none) firstprivate(values, hi, mid2)
		{ _Eidos_ParallelQuicksort_I(values, mid2, hi); }		// Right branch
	}
}
#endif

void Eidos_ParallelQuicksort_I(int64_t *values, int64_t nelements)
{
#ifdef _OPENMP
	if (nelements > 1000) {
		#pragma omp parallel default(none) shared(values, nelements)
		{
			#pragma omp single nowait
			{
				_Eidos_ParallelQuicksort_I(values, 0, nelements - 1);
			}
		} // End of parallel region
	}
	else
	{
		// Use std::sort for small vectors
		std::sort(values, values + nelements);
	}
#else
	// Use std::sort when not running parallel
	std::sort(values, values + nelements);
#endif
}


// This parallel mergesort code is thanks to Libor Bukata and Jan Dvořák, modified from
// https://cw.fel.cvut.cz/old/_media/courses/b4m35pag/lab6_slides_advanced_openmp.pdf
#ifdef _OPENMP
static void _Eidos_ParallelMergesort_I(int64_t *values, int64_t left, int64_t right)
{
	if (left >= right)
		return;
	
	if (right - left <= 1000)
	{
		// fall over to using std::sort when below a threshold interval size
		// the larger the threshold, the less time we spend thrashing tasks on small
		// intervals, which is good; but it also sets a limit on how many threads we
		// we bring to bear on relatively small sorts, which is bad; 1000 seems fine
		std::sort(values + left, values + right + 1);
	}
	else
	{
		int64_t mid = (left + right) / 2;
		#pragma omp taskgroup
		{
			// the original code had if() limits on task subdivision here, but that
			// doesn't make sense to me, because we also have the threshold above,
			// which serves the same purpose but avoids using std::sort on subdivided
			// regions and then merging them with inplace_merge; if we assume that
			// std::sort is faster than mergesort when running on one thread, then
			// merging subdivided std::sort calls only seems like a good strategy
			// when the std::sort calls happen on separate threads
			#pragma omp task default(none) firstprivate(values, left, mid) untied
			_Eidos_ParallelMergesort_I(values, left, mid);
			#pragma omp task default(none) firstprivate(values, mid, right) untied
			_Eidos_ParallelMergesort_I(values, mid + 1, right);
			#pragma omp taskyield
		}
		std::inplace_merge(values + left, values + mid + 1, values + right + 1);
	}
}
#endif

void Eidos_ParallelMergesort_I(int64_t *values, int64_t nelements)
{ 
#ifdef _OPENMP
	if (nelements > 1000) {
		#pragma omp parallel default(none) shared(values, nelements)
		{
			#pragma omp single
			{
				_Eidos_ParallelMergesort_I(values, 0, nelements - 1);
			}
		} // End of parallel region
	}
	else
	{
		// Use std::sort for small vectors
		std::sort(values, values + nelements);
	}
#else
	// Use std::sort when not running parallel
	std::sort(values, values + nelements);
#endif
}






































