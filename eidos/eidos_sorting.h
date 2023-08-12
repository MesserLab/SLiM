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


//
//	Parallel sorting; these use std::sort when we are not running parallel, or for small jobs
//

void Eidos_ParallelQuicksort_I(int64_t *values, int64_t nelements);
void Eidos_ParallelMergesort_I(int64_t *values, int64_t nelements);


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
