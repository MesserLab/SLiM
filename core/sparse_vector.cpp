//
//  sparse_vector.cpp
//  SLiM
//
//  Created by Ben Haller on 4/14/2022.
//  Copyright (c) 2022-2023 Philipp Messer.  All rights reserved.
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
	finished_ = false;
	value_type_ = SparseVectorDataType::kNoData;
	
	columns_ = (uint32_t *)malloc(nnz_capacity_ * sizeof(uint32_t));
	values_ = (sv_value_t *)malloc(nnz_capacity_ * sizeof(sv_value_t));
	
	if (!columns_ || !values_)
		EIDOS_TERMINATION << "ERROR (SparseVector::SparseVector): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	ResizeToFitMaxNNZ(ncols_);
}

SparseVector::~SparseVector(void)
{
	ncols_ = 0;
	nnz_ = 0;
	nnz_capacity_ = 0;
	finished_ = false;
	value_type_ = SparseVectorDataType::kNoData;
	
	free(columns_);
	columns_ = nullptr;
	
	free(values_);
	values_ = nullptr;
}

void SparseVector::ResizeToFitMaxNNZ(uint32_t max_nnz)
{
	// The design of SparseVector is that it always knows up front the maximum number of entries that can be added; it is
	// always the number of columns specified to its constructor or to Reset().  This method resizes the vector proactively
	// to make room for that maximum number of entries.  This allows us to add new entries without checking capacity.  For
	// models with a million individuals, this will cause our capacity to grow to a million entries, but that is trivial.
	if (max_nnz > nnz_capacity_)
	{
		do
			nnz_capacity_ <<= 1;
		while (max_nnz > nnz_capacity_);
		
		columns_ = (uint32_t *)realloc(columns_, nnz_capacity_ * sizeof(uint32_t));
		values_ = (sv_value_t *)realloc(values_, nnz_capacity_ * sizeof(sv_value_t));
		
		if (!columns_ || !values_)
			EIDOS_TERMINATION << "ERROR (SparseVector::_ResizeToFitNNZ): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	}
}

sv_value_t SparseVector::Distance(uint32_t p_column) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distance): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distance): sparse vector is not specialized for distances." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distance): column out of range." << EidosTerminate(nullptr);
	
	// scan for the requested column
	for (uint64_t index = 0; index < nnz_; ++index)
	{
		if (columns_[index] == p_column)
			return values_[index];
	}
	
	// no match found; return infinite distance
	return INFINITY;
}

sv_value_t SparseVector::Strength(uint32_t p_column) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strength): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kStrengths)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strength): sparse vector is not specialized for strengths." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strength): column out of range." << EidosTerminate(nullptr);
	
	// scan for the requested column
	for (uint64_t index = 0; index < nnz_; ++index)
	{
		if (columns_[index] == p_column)
			return values_[index];
	}
	
	// no match found; return zero interaction strength
	return 0;
}

size_t SparseVector::MemoryUsage(void)
{
	return (sizeof(uint32_t) + sizeof(sv_value_t)) * (nnz_capacity_);
}

std::ostream &operator<<(std::ostream &p_outstream, const SparseVector &p_vector)
{
	p_outstream << "SparseVector: " << p_vector.ncols_ << " columns";
	if (!p_vector.finished_)
		p_outstream << " (NOT FINISHED)" << std::endl;
	p_outstream << std::endl;
	
	p_outstream << "   ncols == " << p_vector.ncols_ << std::endl;
	p_outstream << "   nnz == " << p_vector.nnz_ << std::endl;
	p_outstream << "   nnz_capacity == " << p_vector.nnz_capacity_ << std::endl;
	
	p_outstream << "   columns == {";
	for (uint32_t nzi = 0; nzi < p_vector.nnz_; ++nzi)
	{
		if (nzi > 0)
			p_outstream << ", ";
		p_outstream << p_vector.columns_[nzi];
	}
	p_outstream << "}" << std::endl;
	
	p_outstream << "   ";
	
	if (p_vector.value_type_ == SparseVectorDataType::kDistances)			p_outstream << "distances";
	else if (p_vector.value_type_ == SparseVectorDataType::kStrengths)		p_outstream << "strengths";
	else
	{
		p_outstream << "unknown values" << std::endl;
		return p_outstream;
	}
	
	p_outstream << " == {";
	for (uint32_t nzi = 0; nzi < p_vector.nnz_; ++nzi)
	{
		if (nzi > 0)
			p_outstream << ", ";
		p_outstream << p_vector.values_[nzi];
	}
	p_outstream << "}" << std::endl;
	
	return p_outstream;
}
