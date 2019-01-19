//
//  sparse_array.h
//  SLiM
//
//  Created by Ben Haller on 8/17/18.
//  Copyright (c) 2018-2019 Philipp Messer.  All rights reserved.
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

#ifndef sparse_array_h
#define sparse_array_h


#include "slim_global.h"

#include <vector>


/*
 This class provides a sparse array of distance/strength pairs, for use by the InteractionType code.  Each sparse array entry
 contains an interaction distance and strength, kept in separate buffers internally.  If a given interaction is not contained
 by the sparse array (because it is beyond the maximum interaction distance), a distance of INFINITY will be returned, with a
 strength of 0.  Each row of the sparse array contains all of the interaction values *felt* by a given individual; each column
 represents the interactions *exerted* by a given individual.  This way one can quickly read all of the interaction strengths
 felt by a focal individual, which is the typical use case.
 */

// These are the types used to store distances and strengths in SparseArray.  They are defined as float, in order to both cut
// down on memory usage and perhaps to increase speed due to less bytes going to/from memory.  Testing indicates that memory
// usage does go down, but speed is unaffected (a bit puzzlingly); anyway, the memory benefit is worthwhile.  These typedefs
// can be changed to double if float's precision is problematic; everything should just work, although that is not tested.
typedef float sa_distance_t;
typedef float sa_strength_t;

class SparseArray
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	// we store the spare array in CSR format, with an offset for each row
	// see https://medium.com/@jmaxg3/101-ways-to-store-a-sparse-matrix-c7f2bf15a229
	// we do not sort by column within a row; we do a linear search for the column
	uint32_t *row_offsets_;			// offsets into columns/values for each row; for N rows, N+1 entries (extra end entry)
	uint32_t *columns_;				// the column indices for the non-empty values in each row
	sa_distance_t *distances_;		// a distance value for each non-empty entry
	sa_strength_t *strengths_;		// a strength value for each non-empty entry
	
	uint32_t nrows_, ncols_;		// the number of rows and columns; determined at construction time
	uint32_t nrows_set_;			// the number of rows that have been configured (at least partially, during building)
	uint32_t nnz_;					// the number of non-zero entries in the sparse array (also at row_offsets[nrows_set])
	uint32_t nnz_capacity_;			// the number of non-zero entries allocated for at present
	
	bool finished_;					// if true, Finished() has been called and the sparse array is ready to use
	
	void _ResizeToFitNNZ(void);
	inline __attribute__((always_inline)) void ResizeToFitNNZ(void) { if (nnz_ > nnz_capacity_) _ResizeToFitNNZ(); };
	
public:
	SparseArray(const SparseArray&) = delete;					// no copying
	SparseArray& operator=(const SparseArray&) = delete;		// no copying
	SparseArray(void) = delete;									// no null construction
	SparseArray(unsigned int p_nrows, unsigned int p_ncols);
	~SparseArray(void);
	
	void Reset(void);											// reset to a dimensionless state, keeping buffers
	void Reset(unsigned int p_nrows, unsigned int p_ncols);		// reset to new dimensions, keeping buffers
	
	// Building a sparse array; has to be done in row order, and then has to be Finished().  SparseArray supports building
	// a row at a time, or one entry at a time, but one or the other method must be chosen and used throughout the build.
	// Similarly, you can supply just distances and then add strengths later (using InteractionsForRow() to modify the data),
	// or you can build supplying strengths during the build, but you should choose one method or the other and stick with
	// it.  No internal checks are done to guarantee that the build is done using only one method; that is the caller's duty.
	// If you build without strengths, a buffer for strengths is still allocated and realloced, but no values are written
	// to it until you do that yourself with InteractionsForRow().
	void AddRowDistances(uint32_t p_row, const uint32_t *p_columns, const sa_distance_t *p_distances, uint32_t p_row_nnz);
	void AddRowInteractions(uint32_t p_row, const uint32_t *p_columns, const sa_distance_t *p_distances, const sa_strength_t *p_strengths, uint32_t p_row_nnz);
	
	inline void AddEntryDistance(uint32_t p_row, const uint32_t p_column, sa_distance_t p_distance)
	{
#if DEBUG
		if (finished_)
			EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryDistance): (internal error) adding entry to sparse array that is finished." << EidosTerminate(nullptr);
		
		// ensure that we are building sequentially, visiting rows in order but potentially skipping empty rows
		if (p_row + 1 < nrows_set_)
		{
			EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryDistance): (internal error) adding entry out of order." << EidosTerminate(nullptr);
		}
		else if (p_row + 1 > nrows_set_)
		{
			// starting a new row
			if (p_row >= nrows_)
				EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryDistance): (internal error) adding row beyond the end of the sparse array." << EidosTerminate(nullptr);
		}
		// else, adding another entry to the current row
		
		if (p_column >= ncols_)
			EIDOS_TERMINATION << "ERROR (SparseArray::AddEntryDistance): (internal error) adding column beyond the end of the sparse array." << EidosTerminate(nullptr);
#endif
		
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
	}
	void AddEntryInteraction(uint32_t p_row, const uint32_t p_column, sa_distance_t p_distance, sa_strength_t p_strength);
	
	void Finished(void);
	inline __attribute__((always_inline)) bool IsFinished() const { return finished_; };
	
	// Dimensions
	inline __attribute__((always_inline)) uint32_t RowCount() const { return nrows_; };
	inline __attribute__((always_inline)) uint32_t ColumnCount() const { return ncols_; };
	
	inline __attribute__((always_inline)) uint32_t AddedRowCount() const { return nrows_set_; };	// the number of rows that have been (at least partially) added
	
	// Accessing the sparse array	
	sa_distance_t Distance(uint32_t p_row, uint32_t p_column) const;
	sa_strength_t Strength(uint32_t p_row, uint32_t p_column) const;
	void PatchStrength(uint32_t p_row, uint32_t p_column, sa_strength_t p_strength);	// modify a strength after the sparse array has been built
	
	const sa_distance_t *DistancesForRow(uint32_t p_row, uint32_t *p_row_nnz, const uint32_t **p_row_columns) const;
	const sa_strength_t *StrengthsForRow(uint32_t p_row, uint32_t *p_row_nnz, const uint32_t **p_row_columns) const;
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsage(void);
	
	// Non-const access, for filling in strength values after the fact (among other uses)
	void InteractionsForRow(uint32_t p_row, uint32_t *p_row_nnz, uint32_t **p_row_columns, sa_distance_t **p_row_distances, sa_strength_t **p_row_strengths);
	
	friend std::ostream &operator<<(std::ostream &p_outstream, const SparseArray &p_array);
};

std::ostream &operator<<(std::ostream &p_outstream, const SparseArray &p_array);


#endif /* sparse_array_h */
























