//
//  sparse_array.cpp
//  SLiM
//
//  Created by Ben Haller on 8/17/18.
//  Copyright (c) 2018-2020 Philipp Messer.  All rights reserved.
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


#include "sparse_array.h"

#include <ostream>
#include <cmath>
#include <string.h>

#pragma mark -
#pragma mark SparseArray
#pragma mark -

SparseArray::SparseArray(unsigned int p_nrows, unsigned int p_ncols)
{
	if ((p_nrows == 0) || (p_ncols == 0))
		EIDOS_TERMINATION << "ERROR (SparseArray::SparseArray): zero-size sparse array." << EidosTerminate(nullptr);
	
	nrows_ = p_nrows;
	ncols_ = p_ncols;
	nrows_set_ = 0;
	nnz_ = 0;
	nnz_capacity_ = 1024;
	
	row_offsets_ = (uint32_t *)malloc((nrows_ + 1) * sizeof(uint32_t));
	columns_ = (uint32_t *)malloc(nnz_capacity_ * sizeof(uint32_t));
	distances_ = (sa_distance_t *)malloc(nnz_capacity_ * sizeof(sa_distance_t));
	strengths_ = (sa_strength_t *)malloc(nnz_capacity_ * sizeof(sa_strength_t));
	
	row_offsets_[nrows_set_] = 0;
	finished_ = false;
}

SparseArray::~SparseArray(void)
{
	nrows_ = 0;
	ncols_ = 0;
	nrows_set_ = 0;
	nnz_ = 0;
	nnz_capacity_ = 0;
	finished_ = false;
	
	free(row_offsets_);
	row_offsets_ = nullptr;
	
	free(columns_);
	columns_ = nullptr;
	
	free(distances_);
	distances_ = nullptr;
	
	free(strengths_);
	strengths_ = nullptr;
}

void SparseArray::Reset(void)
{
	nrows_ = 0;
	ncols_ = 0;
	nrows_set_ = 0;
	nnz_ = 0;
	finished_ = false;
}

void SparseArray::Reset(unsigned int p_nrows, unsigned int p_ncols)
{
	if ((p_nrows == 0) || (p_ncols == 0))
		EIDOS_TERMINATION << "ERROR (SparseArray::Reset): zero-size sparse array." << EidosTerminate(nullptr);
	
	nrows_ = p_nrows;
	ncols_ = p_ncols;
	nrows_set_ = 0;
	nnz_ = 0;
	
	row_offsets_ = (uint32_t *)realloc(row_offsets_, (nrows_ + 1) * sizeof(uint32_t));
	
	row_offsets_[nrows_set_] = 0;
	finished_ = false;
}

void SparseArray::_ResizeToFitNNZ(void)
{
	if (nnz_ > nnz_capacity_)	// guaranteed if we're called by ResizeToFitNNZ(), but might as well be safe...
	{
		do
			nnz_capacity_ <<= 1;
		while (nnz_ > nnz_capacity_);
		
		columns_ = (uint32_t *)realloc(columns_, nnz_capacity_ * sizeof(uint32_t));
		distances_ = (sa_distance_t *)realloc(distances_, nnz_capacity_ * sizeof(sa_distance_t));
		strengths_ = (sa_strength_t *)realloc(strengths_, nnz_capacity_ * sizeof(sa_strength_t));
	}
}

// BCH 6/6/2020: Note this method is not called from anywhere
void SparseArray::AddRowDistances(uint32_t p_row, const uint32_t *p_columns, const sa_distance_t *p_distances, uint32_t p_row_nnz)
{
	// ensure that we are building sequentially, visiting each row exactly once
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowDistances): adding row to sparse array that is finished." << EidosTerminate(nullptr);
	if (nrows_set_ >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowDistances): adding row to sparse array that is already full." << EidosTerminate(nullptr);
	if (p_row != nrows_set_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowDistances): adding row out of order." << EidosTerminate(nullptr);
	if ((p_row_nnz != 0) && (!p_columns || !p_distances))
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowDistances): null pointer supplied for non-empty row." << EidosTerminate(nullptr);
	
	// make room for the new entries
	nnz_ += p_row_nnz;
	ResizeToFitNNZ();
	
	// copy over the new entries; no bounds check on columns, for speed
	uint32_t offset = row_offsets_[nrows_set_];
	
	row_offsets_[++nrows_set_] = offset + p_row_nnz;
	memcpy(columns_ + offset, p_columns, p_row_nnz * sizeof(uint32_t));
	memcpy(distances_ + offset, p_distances, p_row_nnz * sizeof(sa_distance_t));
}

void SparseArray::AddRowInteractions(uint32_t p_row, const uint32_t *p_columns, const sa_distance_t *p_distances, const sa_strength_t *p_strengths, uint32_t p_row_nnz)
{
	// ensure that we are building sequentially, visiting each row exactly once
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowInteractions): adding row to sparse array that is finished." << EidosTerminate(nullptr);
	if (nrows_set_ >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowInteractions): adding row to sparse array that is already full." << EidosTerminate(nullptr);
	if (p_row != nrows_set_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowInteractions): adding row out of order." << EidosTerminate(nullptr);
	if ((p_row_nnz != 0) && (!p_columns || !p_distances || !p_strengths))
		EIDOS_TERMINATION << "ERROR (SparseArray::AddRowInteractions): null pointer supplied for non-empty row." << EidosTerminate(nullptr);
	
	// make room for the new entries
	nnz_ += p_row_nnz;
	ResizeToFitNNZ();
	
	// copy over the new entries; no bounds check on columns, for speed
	uint32_t offset = row_offsets_[nrows_set_];
	
	row_offsets_[++nrows_set_] = offset + p_row_nnz;
	memcpy(columns_ + offset, p_columns, p_row_nnz * sizeof(uint32_t));
	memcpy(distances_ + offset, p_distances, p_row_nnz * sizeof(sa_distance_t));
	memcpy(strengths_ + offset, p_strengths, p_row_nnz * sizeof(sa_strength_t));
}

void SparseArray::AddEntryInteraction(uint32_t p_row, uint32_t p_column, sa_distance_t p_distance, sa_strength_t p_strength)
{
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryInteraction): adding entry to sparse array that is finished." << EidosTerminate(nullptr);
	
	// ensure that we are building sequentially, visiting rows in order but potentially skipping empty rows
	if (p_row + 1 < nrows_set_)
	{
		EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryInteraction): adding entry out of order." << EidosTerminate(nullptr);
	}
	else if (p_row + 1 > nrows_set_)
	{
		// starting a new row
		if (p_row >= nrows_)
			EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryInteraction): adding row beyond the end of the sparse array." << EidosTerminate(nullptr);
	}
	// else, adding another entry to the current row
	
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryInteraction): adding column beyond the end of the sparse array." << EidosTerminate(nullptr);
	
	// make room for the new entries
	nnz_++;
	ResizeToFitNNZ();
	
	// add intervening empty rows
	uint32_t offset = row_offsets_[nrows_set_];
	
	while (p_row + 1 > nrows_set_)
		row_offsets_[++nrows_set_] = offset;
	
	// insert the new entry
	row_offsets_[nrows_set_] = offset + 1;
	columns_[offset] = p_column;
	distances_[offset] = p_distance;
	strengths_[offset] = p_strength;
}

void SparseArray::Finished(void)
{
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Finished): finishing sparse array that is already finished." << EidosTerminate(nullptr);
	
	uint32_t offset = row_offsets_[nrows_set_];
	
	while (nrows_set_ < nrows_)
		row_offsets_[++nrows_set_] = offset;
	
	finished_ = true;
}

sa_distance_t SparseArray::Distance(uint32_t p_row, uint32_t p_column) const
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Distance): sparse array is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_row >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Distance): row out of range." << EidosTerminate(nullptr);
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Distance): column out of range." << EidosTerminate(nullptr);
	
	// get the offset into columns/values for p_row, and the number of entries for this row
	uint32_t offset = row_offsets_[p_row];
	uint32_t offset_next = row_offsets_[p_row + 1];
	
	// scan for the requested column
	for (uint32_t index = offset; index < offset_next; ++index)
	{
		if (columns_[index] == p_column)
			return distances_[index];
	}
	
	// no match found; return infinite distance
	return INFINITY;
}

sa_strength_t SparseArray::Strength(uint32_t p_row, uint32_t p_column) const
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Strength): sparse array is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_row >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Strength): row out of range." << EidosTerminate(nullptr);
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Strength): column out of range." << EidosTerminate(nullptr);
	
	// get the offset into columns/values for p_row, and the number of entries for this row
	uint32_t offset = row_offsets_[p_row];
	uint32_t offset_next = row_offsets_[p_row + 1];
	
	// scan for the requested column
	for (uint32_t index = offset; index < offset_next; ++index)
	{
		if (columns_[index] == p_column)
			return strengths_[index];
	}
	
	// no match found; return zero interaction strength
	return 0;
}

void SparseArray::PatchStrength(uint32_t p_row, uint32_t p_column, sa_strength_t p_strength)
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::PatchStrength): sparse array is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_row >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::PatchStrength): row out of range." << EidosTerminate(nullptr);
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseArray::PatchStrength): column out of range." << EidosTerminate(nullptr);
	
	// get the offset into columns/values for p_row, and the number of entries for this row
	uint32_t offset = row_offsets_[p_row];
	uint32_t offset_next = row_offsets_[p_row + 1];
	
	// scan for the requested column
	for (uint32_t index = offset; index < offset_next; ++index)
	{
		if (columns_[index] == p_column)
		{
			strengths_[index] = p_strength;
			return;
		}
	}
	
	// no match found
	EIDOS_TERMINATION << "ERROR (SparseArray::PatchStrength): entry does not exist." << EidosTerminate(nullptr);
}

const sa_distance_t *SparseArray::DistancesForRow(uint32_t p_row, uint32_t *p_row_nnz, const uint32_t **p_row_columns) const
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::DistancesForRow): sparse array is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_row >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::DistancesForRow): row out of range." << EidosTerminate(nullptr);
	
	// get the offset into columns/values for p_row, and the number of entries for this row
	uint32_t offset = row_offsets_[p_row];
	uint32_t count = row_offsets_[p_row + 1] - offset;
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_row_nnz = count;
	if (p_row_columns)
		*p_row_columns = columns_ + offset;
	return distances_ + offset;
}

const sa_strength_t *SparseArray::StrengthsForRow(uint32_t p_row, uint32_t *p_row_nnz, const uint32_t **p_row_columns) const
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::StrengthsForRow): sparse array is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_row >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::StrengthsForRow): row out of range." << EidosTerminate(nullptr);
	
	// get the offset into columns/values for p_row, and the number of entries for this row
	uint32_t offset = row_offsets_[p_row];
	uint32_t count = row_offsets_[p_row + 1] - offset;
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_row_nnz = count;
	if (p_row_columns)
		*p_row_columns = columns_ + offset;
	return strengths_ + offset;
}

void SparseArray::InteractionsForRow(uint32_t p_row, uint32_t *p_row_nnz, uint32_t **p_row_columns, sa_distance_t **p_row_distances, sa_strength_t **p_row_strengths)
{
#if DEBUG
	// should be done building the array
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::InteractionsForRow): sparse array is not finished being built." << EidosTerminate(nullptr);
#endif
	
	// bounds-check
	if (p_row >= nrows_)
		EIDOS_TERMINATION << "ERROR (SparseArray::InteractionsForRow): row out of range." << EidosTerminate(nullptr);
	
	// get the offset into columns/values for p_row, and the number of entries for this row
	uint32_t offset = row_offsets_[p_row];
	uint32_t count = row_offsets_[p_row + 1] - offset;
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_row_nnz = count;
	if (p_row_columns)
		*p_row_columns = columns_ + offset;
	if (p_row_distances)
		*p_row_distances = distances_ + offset;
	if (p_row_strengths)
		*p_row_strengths = strengths_ + offset;
}

size_t SparseArray::MemoryUsage(void)
{
	size_t usage = 0;
	
	usage += sizeof(uint32_t) * (nrows_ + 1);
	usage += (sizeof(uint32_t) + sizeof(sa_distance_t) + sizeof(sa_strength_t)) * (nnz_capacity_);
	
	return usage;
}

std::ostream &operator<<(std::ostream &p_outstream, const SparseArray &p_array)
{
	p_outstream << "SparseArray: " << p_array.nrows_set_ << " x " << p_array.ncols_;
	if (!p_array.finished_)
		p_outstream << " (NOT FINISHED)" << std::endl;
	p_outstream << std::endl;
	
	p_outstream << "   nrows == " << p_array.nrows_ << std::endl;
	p_outstream << "   ncols == " << p_array.ncols_ << std::endl;
	p_outstream << "   nrows_set == " << p_array.nrows_set_ << std::endl;
	p_outstream << "   nnz == " << p_array.nnz_ << std::endl;
	p_outstream << "   nnz_capacity == " << p_array.nnz_capacity_ << std::endl;
	
	p_outstream << "   row_offsets == {";
	for (uint32_t row = 0; row < p_array.nrows_set_; ++row)
	{
		if (row > 0)
			p_outstream << ", ";
		p_outstream << p_array.row_offsets_[row];
	}
	p_outstream << "}" << std::endl;
	
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
	
	for (uint32_t row = 0; row < p_array.nrows_set_; ++row)
	{
		for (uint32_t col = 0; col < p_array.ncols_; ++col)
		{
			sa_distance_t distance = p_array.Distance(row, col);
			sa_strength_t strength = p_array.Strength(row, col);
			
			if (std::isfinite(distance))
				p_outstream << "   (" << row << ", " << col << ") == {" << distance << ", " << strength << "}" << std::endl;
		}
	}
	
	return p_outstream;
}






















