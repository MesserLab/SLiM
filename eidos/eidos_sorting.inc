//
//  eidos_sorting.inc
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


// This file is intended to be #included to generate code.  This would be cleaner if we could use
// C++ templates, but that seems to kill performance, in my tests; the compiler does not seem to
// be smart enough, or (very possibly) I'm doing it wrong.  Anyhow, define SORTTYPE and then include.

#ifndef SORTTYPE
#error SORTTYPE must be defined before including this file
#endif


// This parallel quicksort code is thanks to Ruud van der Pas, modified from
// https://www.openmp.org/wp-content/uploads/sc16-openmp-booth-tasking-ruud.pdf
#ifdef _OPENMP
static void _Eidos_ParallelQuicksort_ASCENDING(SORTTYPE *values, int64_t lo, int64_t hi, int64_t fallthrough)
{
	if (lo >= hi)
		return;
	
	if (hi - lo + 1 <= fallthrough) {
		// fall through to sorting with std::sort() below our threshold size
		std::sort(values + lo, values + hi + 1);
	} else {
		// choose the middle of three pivots, in an attempt to avoid really bad pivots
		SORTTYPE &pivot1 = *(values + lo);
		SORTTYPE &pivot2 = *(values + hi);
		SORTTYPE &pivot3 = *(values + ((lo + hi) >> 1));
		SORTTYPE pivot;
		
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
		SORTTYPE *middle1 = std::partition(values + lo, values + hi + 1, [pivot](const SORTTYPE& em) { return em < pivot; });
		SORTTYPE *middle2 = std::partition(middle1, values + hi + 1, [pivot](const SORTTYPE& em) { return !(pivot < em); });
		int64_t mid1 = middle1 - values;
		int64_t mid2 = middle2 - values;
		#pragma omp task default(none) firstprivate(values, lo, mid1, fallthrough)
		{ _Eidos_ParallelQuicksort_ASCENDING(values, lo, mid1 - 1, fallthrough); }	// Left branch
		#pragma omp task default(none) firstprivate(values, hi, mid2, fallthrough)
		{ _Eidos_ParallelQuicksort_ASCENDING(values, mid2, hi, fallthrough); }		// Right branch
	}
}

static void _Eidos_ParallelQuicksort_DESCENDING(SORTTYPE *values, int64_t lo, int64_t hi, int64_t fallthrough)
{
	if (lo >= hi)
		return;
	
	if (hi - lo + 1 <= fallthrough) {
		// fall through to sorting with std::sort() below our threshold size
		// note that descending sorts are slower because this STL usage pattern is slower!
		std::sort(values + lo, values + hi + 1, std::greater<SORTTYPE>());
	} else {
		// choose the middle of three pivots, in an attempt to avoid really bad pivots
		SORTTYPE &pivot1 = *(values + lo);
		SORTTYPE &pivot2 = *(values + hi);
		SORTTYPE &pivot3 = *(values + ((lo + hi) >> 1));
		SORTTYPE pivot;
		
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
		SORTTYPE *middle1 = std::partition(values + lo, values + hi + 1, [pivot](const SORTTYPE& em) { return em > pivot; });
		SORTTYPE *middle2 = std::partition(middle1, values + hi + 1, [pivot](const SORTTYPE& em) { return !(pivot > em); });
		int64_t mid1 = middle1 - values;
		int64_t mid2 = middle2 - values;
		#pragma omp task default(none) firstprivate(values, lo, mid1, fallthrough)
		{ _Eidos_ParallelQuicksort_DESCENDING(values, lo, mid1 - 1, fallthrough); }	// Left branch
		#pragma omp task default(none) firstprivate(values, hi, mid2, fallthrough)
		{ _Eidos_ParallelQuicksort_DESCENDING(values, mid2, hi, fallthrough); }		// Right branch
	}
}
#endif

void Eidos_ParallelSort(SORTTYPE *values, int64_t nelements, bool p_ascending)
{
	// This wrapper just ensures that we use std::sort for small vectors,
	// or when not parallel, andhandles ascending vs. descending
#ifdef _OPENMP
	if (nelements >= EIDOS_SORT_CUTOFF) {
		EIDOS_THREAD_COUNT(EIDOS_SORT_THREADS);
#pragma omp parallel default(none) shared(values, nelements, p_ascending) num_threads(thread_count)
		{
			// We fall through to using std::sort when below a threshold interval size.
			// The larger the threshold, the less time we spend thrashing tasks on small
			// intervals, which is good; but it also sets a limit on how many threads we
			// we bring to bear on relatively small sorts, which is bad.  We try to
			// calculate the optimal fall-through heuristically here; basically we want
			// to subdivide with tasks enough that the workload is shared well, and then
			// do the rest of the work with std::sort().  The more threads there are,
			// the smaller we want to subdivide.
			int64_t fallthrough = nelements / (EIDOS_FALLTHROUGH_FACTOR * omp_get_num_threads());
			
			if (fallthrough < 1000)
				fallthrough = 1000;
			
			if (p_ascending)
			{
#pragma omp single nowait
				{
					_Eidos_ParallelQuicksort_ASCENDING(values, 0, nelements - 1, fallthrough);
				}
			}
			else
			{
#pragma omp single nowait
				{
					_Eidos_ParallelQuicksort_DESCENDING(values, 0, nelements - 1, fallthrough);
				}
			}
		} // End of parallel region
	}
	else
	{
		if (p_ascending)
			std::sort(values, values + nelements);
		else
			std::sort(values, values + nelements, std::greater<SORTTYPE>());
	}
#else
	if (p_ascending)
		std::sort(values, values + nelements);
	else
		std::sort(values, values + nelements, std::greater<SORTTYPE>());
#endif
}

// Leave the includer with SORTTYPE undefined, to avoid accidents
#undef SORTTYPE
#undef EIDOS_SORT_CUTOFF
#undef EIDOS_SORT_THREADS




































