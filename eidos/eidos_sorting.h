//
//  eidos_sorting.h
//  Eidos
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

/*
 
 This file contains an assortment of sorting algorithms used for various purposes.
 
 */


#ifndef __Eidos__eidos_sorting_h
#define __Eidos__eidos_sorting_h

#include "eidos_openmp.h"

#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <string>
#include <cmath>


//
//	Parallel sorting; these use std::sort when we are not running parallel, or for small jobs
//

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

void Eidos_ParallelSort(int64_t *values, int64_t nelements, bool p_ascending);
void Eidos_ParallelSort(float *values, int64_t nelements, bool p_ascending);
void Eidos_ParallelSort(double *values, int64_t nelements, bool p_ascending);
void Eidos_ParallelSort(std::string *values, int64_t nelements, bool p_ascending);

// This constant basically determines how much a sorting task will get subdivided
// before the small chunks get turned over to std::sort() to single-threaded sort.
// The optimum (probably hardware-dependent) was determined by trial and error.
#define EIDOS_FALLTHROUGH_FACTOR	10

// This template-based version should be equivalent to the above include-based
// version, but unfortunately it runs a little slower; I can't figure out why.
#ifdef _OPENMP
template <typename T, typename Compare>
static void _Eidos_ParallelQuicksort_Comparator(T *values, int64_t lo, int64_t hi, const Compare &comparator, int64_t fallthrough)
{
	if (lo >= hi)
		return;
	
	if (hi - lo + 1 <= fallthrough) {
		// fall through to sorting with std::sort() below our threshold size
		std::sort(values + lo, values + hi + 1, comparator);
	} else {
		// choose the middle of three pivots, in an attempt to avoid really bad pivots
		T &pivot1 = *(values + lo);
		T &pivot2 = *(values + hi);
		T &pivot3 = *(values + ((lo + hi) >> 1));
		T pivot;
		
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
		T *middle1 = std::partition(values + lo, values + hi + 1, [&, pivot, comparator](const T& em) { return comparator(em, pivot); });
		T *middle2 = std::partition(middle1, values + hi + 1, [&, pivot, comparator](const T& em) { return !comparator(pivot, em); });
		int64_t mid1 = middle1 - values;
		int64_t mid2 = middle2 - values;
		#pragma omp task default(none) firstprivate(values, lo, mid1) shared(comparator, fallthrough)
		{ _Eidos_ParallelQuicksort_Comparator(values, lo, mid1 - 1, comparator, fallthrough); }	// Left branch
		#pragma omp task default(none) firstprivate(values, hi, mid2) shared(comparator, fallthrough)
		{ _Eidos_ParallelQuicksort_Comparator(values, mid2, hi, comparator, fallthrough); }		// Right branch
	}
}
#endif

template <typename T, typename Compare>
void Eidos_ParallelSort_Comparator(T *values, int64_t nelements, const Compare &comparator)
{
	// This wrapper just ensures that we use std::sort for small vectors,
	// or when not parallel, andhandles ascending vs. descending
#ifdef _OPENMP
	if (nelements >= EIDOS_OMPMIN_SORT_INT) {				// we use the integer cutoff here; it's the common case
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_SORT_INT);	// we use the integer cutoff here; it's the common case
#pragma omp parallel default(none) shared(values, nelements, comparator) num_threads(thread_count)
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
			
#pragma omp single nowait
			{
				_Eidos_ParallelQuicksort_Comparator(values, 0, nelements - 1, comparator, fallthrough);
			}
		} // End of parallel region
	}
	else
	{
		std::sort(values, values + nelements, comparator);
	}
#else
	std::sort(values, values + nelements, comparator);
#endif
}


//
//	Sriram's parallel sort
//

template <typename T, typename Compare>
void Sriram_parallel_omp_sort(std::vector<T> &data, Compare comparator)
{
	int num_threads = omp_get_max_threads();
	size_t size = data.size();
	size_t chunk_size = size / num_threads;		// chunk size per thread
	
#pragma omp parallel
	{
		int tid = omp_get_thread_num();
		// cout << omp_get_thread_num() << std::endl;
		
		size_t start = tid * chunk_size;
		size_t end = (tid + 1) * chunk_size;
		
		if (tid == num_threads - 1)
			end = size;
		
		// sort sub-array using comparator
		std::sort(data.begin() + start, data.begin() + end, comparator);
	}
	
	std::sort(data.begin(), data.end(), comparator);
}


//
//	Get indexes that would result in sorted ordering of a vector.
//	This rather nice code is adapted from http://stackoverflow.com/a/12399290/2752221
//

template <typename T>
std::vector<int64_t> EidosSortIndexes(const std::vector<T> &p_v, bool p_ascending = true)
{
	// initialize original index locations
	std::vector<int64_t> idx(p_v.size());
	std::iota(idx.begin(), idx.end(), 0);
	
	// sort indexes based on comparing values in v
	if (p_ascending)
		std::sort(idx.begin(), idx.end(), [&p_v](int64_t i1, int64_t i2) {return p_v[i1] < p_v[i2];});
	else
		std::sort(idx.begin(), idx.end(), [&p_v](int64_t i1, int64_t i2) {return p_v[i1] > p_v[i2];});
	
	return idx;
}

template <>
inline std::vector<int64_t> EidosSortIndexes<double>(const std::vector<double> &p_v, bool p_ascending)
{
	// initialize original index locations
	std::vector<int64_t> idx(p_v.size());
	std::iota(idx.begin(), idx.end(), 0);
	
	// sort indexes based on comparing values in v
	// this specialization for type double sorts NaNs to the end
	if (p_ascending)
		std::sort(idx.begin(), idx.end(), [&p_v](int64_t i1, int64_t i2) {return std::isnan(p_v[i2]) || (p_v[i1] < p_v[i2]);});
	else
		std::sort(idx.begin(), idx.end(), [&p_v](int64_t i1, int64_t i2) {return std::isnan(p_v[i2]) || (p_v[i1] > p_v[i2]);});
	
	return idx;
}

template <typename T>
std::vector<int64_t> EidosSortIndexes(const T *p_v, size_t p_size, bool p_ascending = true)
{
	// initialize original index locations
	std::vector<int64_t> idx(p_size);
	std::iota(idx.begin(), idx.end(), 0);
	
	// sort indexes based on comparing values in v
	if (p_ascending)
		std::sort(idx.begin(), idx.end(), [p_v](int64_t i1, int64_t i2) {return p_v[i1] < p_v[i2];});
	else
		std::sort(idx.begin(), idx.end(), [p_v](int64_t i1, int64_t i2) {return p_v[i1] > p_v[i2];});
	
	return idx;
}

template <>
inline std::vector<int64_t> EidosSortIndexes<double>(const double *p_v, size_t p_size, bool p_ascending)
{
	// initialize original index locations
	std::vector<int64_t> idx(p_size);
	std::iota(idx.begin(), idx.end(), 0);
	
	// sort indexes based on comparing values in v
	// this specialization for type double sorts NaNs to the end
	if (p_ascending)
		std::sort(idx.begin(), idx.end(), [p_v](int64_t i1, int64_t i2) {return std::isnan(p_v[i2]) || (p_v[i1] < p_v[i2]);});
	else
		std::sort(idx.begin(), idx.end(), [p_v](int64_t i1, int64_t i2) {return std::isnan(p_v[i2]) || (p_v[i1] > p_v[i2]);});
	
	return idx;
}


#endif /* __Eidos__eidos_sorting_h */
