//
//  sparse_vector.cpp
//  SLiM
//
//  Created by Ben Haller on 4/14/2022.
//  Copyright (c) 2022 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "sparse_vector.h"

#include <ostream>
#include <cmath>
#include <string.h>

#pragma mark -
#pragma mark SparseVector
#pragma mark -

SparseVector::SparseVector(unsigned int p_ncols)
{
	if (p_ncols == 0)
		EIDOS_TERMINATION << "ERROR (SparseVector::SparseVector): zero-size sparse vector." << EidosTerminate(nullptr);
	
	ncols_ = p_ncols;
	nnz_ = 0;
	nnz_capacity_ = 1024;
	
	columns_ = (uint32_t *)malloc(nnz_capacity_ * sizeof(uint32_t));
	distances_ = (sv_distance_t *)malloc(nnz_capacity_ * sizeof(sv_distance_t));
	strengths_ = (sv_strength_t *)malloc(nnz_capacity_ * sizeof(sv_strength_t));
	
	if (!columns_ || !distances_ || !strengths_)
		EIDOS_TERMINATION << "ERROR (SparseVector::SparseVector): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	finished_ = false;
}

SparseVector::~SparseVector(void)
{
	ncols_ = 0;
	nnz_ = 0;
	nnz_capacity_ = 0;
	finished_ = false;
	
	free(columns_);
	columns_ = nullptr;
	
	free(distances_);
	distances_ = nullptr;
	
	free(strengths_);
	strengths_ = nullptr;
}

void SparseVector::_ResizeToFitNNZ(void)
{
	if (nnz_ > nnz_capacity_)	// guaranteed if we're called by ResizeToFitNNZ(), but might as well be safe...
	{
		do
			nnz_capacity_ <<= 1;
		while (nnz_ > nnz_capacity_);
		
		columns_ = (uint32_t *)realloc(columns_, nnz_capacity_ * sizeof(uint32_t));
		distances_ = (sv_distance_t *)realloc(distances_, nnz_capacity_ * sizeof(sv_distance_t));
		strengths_ = (sv_strength_t *)realloc(strengths_, nnz_capacity_ * sizeof(sv_strength_t));
		
		if (!columns_ || !distances_ || !strengths_)
			EIDOS_TERMINATION << "ERROR (SparseVector::_ResizeToFitNNZ): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	}
}

sv_distance_t SparseVector::Distance(uint32_t p_column) const
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distance): sparse vector is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distance): column out of range." << EidosTerminate(nullptr);
	
	// scan for the requested column
	for (uint64_t index = 0; index < nnz_; ++index)
	{
		if (columns_[index] == p_column)
			return distances_[index];
	}
	
	// no match found; return infinite distance
	return INFINITY;
}

sv_strength_t SparseVector::Strength(uint32_t p_column) const
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strength): sparse vector is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strength): column out of range." << EidosTerminate(nullptr);
	
	// scan for the requested column
	for (uint64_t index = 0; index < nnz_; ++index)
	{
		if (columns_[index] == p_column)
			return strengths_[index];
	}
	
	// no match found; return zero interaction strength
	return 0;
}

size_t SparseVector::MemoryUsage(void)
{
	return (sizeof(uint32_t) + sizeof(sv_distance_t) + sizeof(sv_strength_t)) * (nnz_capacity_);
}

std::ostream &operator<<(std::ostream &p_outstream, const SparseVector &p_array)
{
	p_outstream << "SparseVector: " << p_array.ncols_ << " columns";
	if (!p_array.finished_)
		p_outstream << " (NOT FINISHED)" << std::endl;
	p_outstream << std::endl;
	
	p_outstream << "   ncols == " << p_array.ncols_ << std::endl;
	p_outstream << "   nnz == " << p_array.nnz_ << std::endl;
	p_outstream << "   nnz_capacity == " << p_array.nnz_capacity_ << std::endl;
	
	p_outstream << "   columns == {";
	for (uint32_t nzi = 0; nzi < p_array.nnz_; ++nzi)
	{
		if (nzi > 0)
			p_outstream << ", ";
		p_outstream << p_array.columns_[nzi];
	}
	p_outstream << "}" << std::endl;
	
	p_outstream << "   values == {";
	for (uint32_t nzi = 0; nzi < p_array.nnz_; ++nzi)
	{
		if (nzi > 0)
			p_outstream << ", ";
		p_outstream << "[" << p_array.distances_[nzi] << ", " << p_array.strengths_[nzi] << "]";
	}
	p_outstream << "}" << std::endl;
	
	if (!p_array.finished_)
		return p_outstream;
	
	return p_outstream;
}
