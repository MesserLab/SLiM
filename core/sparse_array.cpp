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
	greatest_nrows = 0;
	finished_ = false;
    /*Columns and Distances are now ragged 2-D vectors with each row corresponding to one focal individual. 
    This negates the need for row offsets.*/
    initial_width=1;
    columns = (uint32_t **) calloc(nrows_, sizeof(uint32_t*));
    distances = (sa_distance_t **) calloc(nrows_, sizeof(sa_distance_t*));
    strengths = (sa_strength_t **) calloc(nrows_, sizeof(sa_strength_t*));
    nnz = (uint32_t *) calloc(nrows_, sizeof(uint32_t));
    nnz_capacity = (uint32_t *) calloc(nrows_, sizeof(uint32_t));
}

SparseArray::~SparseArray(void)
{
	nrows_ = 0;
	ncols_ = 0;
	finished_ = false;
	greatest_nrows = 0;
	//Deallocate multithreading parts
	free(columns);
    columns = nullptr;

    free(distances);
    distances = nullptr;

    free(strengths);
    strengths=nullptr;

    free(nnz);
    nnz=nullptr;

    free(nnz_capacity);
    nnz_capacity=nullptr;

}

void SparseArray::Reset(void)
{
	EIDOS_BZERO(nnz, sizeof(uint32_t) * nrows_);
	nrows_ = 0;
	ncols_ = 0;
	finished_ = false;

}

void SparseArray::Reset(unsigned int p_nrows, unsigned int p_ncols)
{
	if ((p_nrows == 0) || (p_ncols == 0))
		EIDOS_TERMINATION << "ERROR (SparseArray::Reset): zero-size sparse array." << EidosTerminate(nullptr);
	
	nrows_ = p_nrows;
	ncols_ = p_ncols;

	if(greatest_nrows < nrows_)
		IncreaseNumOfRows(nrows_);
	finished_ = false;
	
}


void SparseArray::IncreaseNumOfRows(uint32_t row_size)
{
	nnz = (uint32_t *)realloc(nnz, (row_size + 1) * sizeof(uint32_t));
	nnz_capacity = (uint32_t *)realloc(nnz_capacity, (row_size + 1) * sizeof(uint32_t));
	//Initializing the nnz and capacity of extended rows to 0
	uint32_t diff = row_size - greatest_nrows;
	memset((nnz + greatest_nrows + 1), 0, diff * sizeof(uint32_t));
	memset((nnz_capacity + greatest_nrows + 1), 0, diff * sizeof(uint32_t));

	columns = (uint32_t **)realloc(columns,  (row_size + 1)  * sizeof(uint32_t*));
    if(columns == NULL)
		EIDOS_TERMINATION << "ERROR (SparseArray::IncreaseNumOfRows): Cannot realloc" << EidosTerminate(nullptr);

	distances = (sa_distance_t **)realloc(distances, (row_size + 1) * sizeof(sa_distance_t*)); 
	if(distances == NULL)
		EIDOS_TERMINATION << "ERROR (SparseArray::IncreaseNumOfRows): Cannot realloc" << EidosTerminate(nullptr);

	strengths = (sa_strength_t **)realloc(strengths, (row_size + 1) * sizeof(sa_strength_t*));
	if(strengths == NULL)
		EIDOS_TERMINATION << "ERROR (SparseArray::IncreaseNumOfRows): Cannot realloc" << EidosTerminate(nullptr);

	greatest_nrows = row_size;
}

void SparseArray::IncreaseRowCapacity(uint32_t p_row)
{
	if(nnz[p_row] == 0)
	{
		//row needs to be initialized before realloc 
		columns[p_row] = nullptr;
		distances[p_row] = nullptr;
		strengths[p_row] = nullptr;
	}

	//vectors are reallocated by doubling up each time we resize
    nnz_capacity[p_row]= (nnz_capacity[p_row]==0) ? initial_width : nnz_capacity[p_row] * 2; //Initialize or double the capacity 

    columns[p_row] = (uint32_t *)realloc(columns[p_row],  nnz_capacity[p_row] * sizeof(uint32_t));
    if(columns[p_row] == NULL)
    	EIDOS_TERMINATION << "ERROR (SparseArray::IncreaseRowCapacity): Cannot realloc." << EidosTerminate(nullptr);

    distances[p_row] = (sa_distance_t *)realloc(distances[p_row], nnz_capacity[p_row] * sizeof(sa_distance_t));
    if(distances[p_row] == NULL)
    	EIDOS_TERMINATION << "ERROR (SparseArray::IncreaseRowCapacity): Cannot realloc." << EidosTerminate(nullptr);

    strengths[p_row] = (sa_strength_t *)realloc(strengths[p_row], nnz_capacity[p_row] * sizeof(sa_strength_t));
    if(strengths[p_row] == NULL)
    	EIDOS_TERMINATION << "ERROR (SparseArray::IncreaseRowCapacity): Cannot realloc." << EidosTerminate(nullptr);

}



void SparseArray::Finished(void)
{
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseArray::Finished): finishing sparse array that is already finished." << EidosTerminate(nullptr);
	
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
	
	// scan for the requested column
	for (uint32_t index = 0; index < nnz[p_row]; ++index)
	{
		if (columns[p_row][index] == p_column)
			return distances[p_row][index];
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
	
	// scan for the requested column
	for (uint32_t index = 0; index < nnz[p_row]; ++index)
	{
		if (columns[p_row][index] == p_column)
			return strengths[p_row][index];
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
	
	/* Instead of iterating between offsets of two rows, we iterate through the number of nnz for that specific row*/ 
	// scan for the requested column
	for (uint32_t index = 0; index < nnz[p_row]; ++index)
	{
		if (columns[p_row][index] == p_column)
		{
			strengths[p_row][index] = p_strength;
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
	//uint32_t offset = row_offsets_[p_row];
	uint32_t count = nnz[p_row];
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_row_nnz = count;
	if (p_row_columns)
		*p_row_columns = columns[p_row];
	return distances[p_row];
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
	// Offsets are now specific row indices
	uint32_t offset = p_row;
		
	// return info; note that a non-null pointer is returned even if count==0
	*p_row_nnz = nnz[p_row];
	if (p_row_columns)
		*p_row_columns = columns[offset];
	return strengths[offset];
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
	// Offsets are now specific row indices
	uint32_t offset = p_row;
	//uint32_t count = row_offsets_[p_row + 1] - offset;
	
	// return info; note that a non-null pointer is returned even if count==0
	//Over here, p_row_columns would return a pointer to the start of the specific row from the columns array. Likewise for distances and strengths
	*p_row_nnz = nnz[p_row];
	if (p_row_columns)
		*p_row_columns = columns[offset];
	if (p_row_distances)
		*p_row_distances = distances[offset];
	if (p_row_strengths)
		*p_row_strengths = strengths[offset];
}


size_t SparseArray::MemoryUsage(void)
{
	size_t total_usage = 0, distance_usage = 0, strength_usage = 0, columns_usage = 0, nnz_usage = 0, nnz_capacity_usage = 0;

	for(int i=0; i<nrows_;i++)
	{
		distance_usage += sizeof(uint32_t) * nnz[i];
		strength_usage += sizeof(uint32_t) * nnz[i];
		columns_usage += sizeof(uint32_t) * nnz[i];
	}
	nnz_usage = sizeof(uint32_t) * nrows_;
	nnz_capacity_usage = sizeof(uint32_t) * nrows_;

	total_usage = distance_usage + strength_usage + columns_usage + nnz_usage + nnz_capacity_usage;
	
	return total_usage;
}

std::ostream &operator<<(std::ostream &p_outstream, const SparseArray &p_array)
{
	if (!p_array.finished_)
		p_outstream << " (NOT FINISHED)" << std::endl;
	p_outstream << std::endl;
	
	p_outstream << "   nrows == " << p_array.nrows_ << std::endl;
	p_outstream << "   ncols == " << p_array.ncols_ << std::endl;

	p_outstream << "   nnz == {";
	for (uint32_t row = 0; row < p_array.nrows_; ++row)
	{
		if (row > 0)
			p_outstream << ", ";
		p_outstream << p_array.nnz[row];
	}
	p_outstream << "}" << std::endl;

	p_outstream << "   nnz_capacity == {";
	for (uint32_t row = 0; row < p_array.nrows_; ++row)
	{
		if (row > 0)
			p_outstream << ", ";
		p_outstream << p_array.nnz_capacity[row];
	}
	p_outstream << "}" << std::endl;
	
	p_outstream << "   columns == {";
	for (uint32_t row = 0; row < p_array.nrows_; ++row)
	{
		for (uint32_t col = 0; col < p_array.nnz[row]; ++col)
		{
			p_outstream << p_array.columns[row][col];
		}
		p_outstream << ", ";
	}
	p_outstream << "}" << std::endl;

	p_outstream << "   distances == {";
	for (uint32_t row = 0; row < p_array.nrows_; ++row)
	{
		for (uint32_t col = 0; col < p_array.nnz[row]; ++col)
		{
			p_outstream << p_array.distances[row][col];
		}
		p_outstream << ", ";
	}
	p_outstream << "}" << std::endl;

	p_outstream << "   strengths == {";
	for (uint32_t row = 0; row < p_array.nrows_; ++row)
	{
		for (uint32_t col = 0; col < p_array.nnz[row]; ++col)
		{
			p_outstream << p_array.strengths[row][col];
		}
		p_outstream << ", ";
	}
	p_outstream << "}" << std::endl;
	
	return p_outstream;
}






















