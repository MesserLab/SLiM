//
//  sparse_vector.h
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

#ifndef sparse_vector_h
#define sparse_vector_h


#include "slim_globals.h"

#include <vector>


/*
 This class provides a sparse vector of distance/strength pairs, for use by the InteractionType code.  Each sparse vector entry
 contains an interaction distance and strength, kept in separate buffers internally.  If a given interaction is not contained
 by the sparse vector (because it is beyond the maximum interaction distance), a distance of INFINITY will be returned, with a
 strength of 0.  A sparse vector contains all of the interaction values *felt* by a given individual (the "receiver"); each column
 represents the interactions *exerted* by particular individuals (the "exerters").  This way one can quickly read all of the
 interaction strengths felt by a focal receiver individual, which is the typical use case.
 */

// This is the type used to store distances and strengths in SparseVector.  It is defined as float, in order both to cut
// down on memory usage, and to maybe increase speed due to vectorization and less bytes going to/from memory.  These typedefs
// can be changed to double if float's precision is problematic; everything should just work, although that is not tested.
typedef float sv_value_t;

// This enum designates the type of value being stored by SparseVector.  It is used for consistency checking in DEBUG builds.
typedef enum {
	kNoData = 0,
	kPresences,
	kDistances,
	kStrengths
} SparseVectorDataType;

class SparseVector
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	// note that we do not sort by column within the row; we do a linear search for the column
	// usually we do not need to identify a particular column, we just want to look at all the values
	sv_value_t *values_;				// a distance or strength value for each non-empty entry
	uint32_t *columns_;					// the column indices for the non-empty values in each row
	SparseVectorDataType value_type_;	// what kind of values we're storing
	
	uint32_t ncols_;					// the number of columns; determined at construction time
	uint32_t nnz_;						// the number of non-zero entries in the sparse vector
	uint32_t nnz_capacity_;				// the number of non-zero entries allocated for at present
	
	bool finished_;						// if true, Finished() has been called and the vector is ready to use
	
	void ResizeToFitMaxNNZ(uint32_t max_nnz);
	
public:
	SparseVector(const SparseVector&) = delete;					// no copying
	SparseVector& operator=(const SparseVector&) = delete;		// no copying
	SparseVector(void) = delete;								// no null construction
	SparseVector(unsigned int p_ncols);
	~SparseVector(void);
	
	void Reset(unsigned int p_ncols, SparseVectorDataType data_type);		// reset to new dimensions
	
	// Building a sparse vector has to be done in column order, one entry at a time, and then has to be Finished().
	// You can supply either distances or strengths; SparseVector does not store both simultaneously.  You should
	// declare in advance which type of value you intend to store; this is checked when building DEBUG.
	void AddEntryPresence(const uint32_t p_column);
	void AddEntryDistance(const uint32_t p_column, sv_value_t p_distance);
	void AddEntryStrength(const uint32_t p_column, sv_value_t p_strength);
	void Finished(void);
	
	inline __attribute__((always_inline)) bool IsFinished() const						{ return finished_; };
	inline __attribute__((always_inline)) uint32_t ColumnCount() const					{ return ncols_; };
	
	inline __attribute__((always_inline)) SparseVectorDataType DataType(void) const		{ return value_type_; }
	inline __attribute__((always_inline)) void SetDataType(SparseVectorDataType type)	{ value_type_ = type; }
	
	// Access to the sparse vector's data
	void Presences(uint32_t *p_nnz) const;
	void Presences(uint32_t *p_nnz, const uint32_t **p_columns) const;
	
	const sv_value_t *Distances(uint32_t *p_nnz) const;
	const sv_value_t *Distances(uint32_t *p_nnz, const uint32_t **p_columns) const;
	void Distances(uint32_t *p_nnz, uint32_t **p_columns, sv_value_t **p_distances);	// non-const
	
	const sv_value_t *Strengths(uint32_t *p_nnz) const;
	const sv_value_t *Strengths(uint32_t *p_nnz, const uint32_t **p_columns) const;
	void Strengths(uint32_t *p_nnz, uint32_t **p_columns, sv_value_t **p_strengths);	// non-const
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsage(void);
	
	friend std::ostream &operator<<(std::ostream &p_outstream, const SparseVector &p_vector);
};

inline void SparseVector::Reset(unsigned int p_ncols, SparseVectorDataType data_type)
{
#if DEBUG
	if (p_ncols == 0)
		EIDOS_TERMINATION << "ERROR (SparseVector::Reset): zero-size sparse vector." << EidosTerminate(nullptr);
#endif
	
	ncols_ = p_ncols;
	nnz_ = 0;
	finished_ = false;
	value_type_ = data_type;
	ResizeToFitMaxNNZ(ncols_);
}

inline void SparseVector::AddEntryPresence(const uint32_t p_column)
{
#if DEBUG
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryPresence): adding entry to sparse vector that is finished." << EidosTerminate(nullptr);
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryPresence): adding column beyond the end of the sparse vector." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kPresences)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryPresence): sparse vector is not specialized for presences." << EidosTerminate(nullptr);
	if (nnz_ >= nnz_capacity_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryPresence): insufficient capacity allocated." << EidosTerminate(nullptr);
#endif
	
	// insert the new entry
	columns_[nnz_] = p_column;
	nnz_++;
}

inline void SparseVector::AddEntryDistance(const uint32_t p_column, sv_value_t p_distance)
{
#if DEBUG
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryDistance): adding entry to sparse vector that is finished." << EidosTerminate(nullptr);
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryDistance): adding column beyond the end of the sparse vector." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryDistance): sparse vector is not specialized for distances." << EidosTerminate(nullptr);
	if (nnz_ >= nnz_capacity_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryDistance): insufficient capacity allocated." << EidosTerminate(nullptr);
#endif
	
	// insert the new entry
	columns_[nnz_] = p_column;
	values_[nnz_] = p_distance;
	nnz_++;
}

inline void SparseVector::AddEntryStrength(const uint32_t p_column, sv_value_t p_strength)
{
#if DEBUG
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryStrength): adding entry to sparse vector that is finished." << EidosTerminate(nullptr);
	if (p_column >= ncols_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryStrength): adding column beyond the end of the sparse vector." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kStrengths)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryStrength): sparse vector is not specialized for strengths." << EidosTerminate(nullptr);
	if (nnz_ >= nnz_capacity_)
		EIDOS_TERMINATION << "ERROR (SparseVector::AddEntryStrength): insufficient capacity allocated." << EidosTerminate(nullptr);
#endif
	
	// insert the new entry
	columns_[nnz_] = p_column;
	values_[nnz_] = p_strength;
	nnz_++;
}

inline __attribute__((always_inline)) void SparseVector::Finished(void)
{
#if DEBUG
	if (finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Finished): finishing sparse vector that is already finished." << EidosTerminate(nullptr);
#endif
	
	if (value_type_ == SparseVectorDataType::kNoData)
		EIDOS_TERMINATION << "ERROR (SparseVector::Finished): sparse vector was never specialized to presences, distances, or strengths." << EidosTerminate(nullptr);
	
	finished_ = true;
}

inline void SparseVector::Presences(uint32_t *p_nnz) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Presences): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kPresences)
		EIDOS_TERMINATION << "ERROR (SparseVector::Presences): sparse vector is not specialized for presences." << EidosTerminate(nullptr);
#endif
	
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
}

inline void SparseVector::Presences(uint32_t *p_nnz, const uint32_t **p_columns) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Presences): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kPresences)
		EIDOS_TERMINATION << "ERROR (SparseVector::Presences): sparse vector is not specialized for presences." << EidosTerminate(nullptr);
#endif
	
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
	*p_columns = columns_;
}

inline const sv_value_t *SparseVector::Distances(uint32_t *p_nnz) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distances): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distances): sparse vector is not specialized for distances." << EidosTerminate(nullptr);
#endif
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
	return values_;
}

inline const sv_value_t *SparseVector::Distances(uint32_t *p_nnz, const uint32_t **p_columns) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distances): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distances): sparse vector is not specialized for distances." << EidosTerminate(nullptr);
#endif
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
	*p_columns = columns_;
	return values_;
}

inline void SparseVector::Distances(uint32_t *p_nnz, uint32_t **p_columns, sv_value_t **p_distances)
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distances): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (SparseVector::Distances): sparse vector is not specialized for distances." << EidosTerminate(nullptr);
#endif
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
	*p_columns = columns_;
	*p_distances = values_;
}

inline const sv_value_t *SparseVector::Strengths(uint32_t *p_nnz) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strengths): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kStrengths)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strengths): sparse vector is not specialized for strengths." << EidosTerminate(nullptr);
#endif
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
	return values_;
}

inline const sv_value_t *SparseVector::Strengths(uint32_t *p_nnz, const uint32_t **p_columns) const
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strengths): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kStrengths)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strengths): sparse vector is not specialized for strengths." << EidosTerminate(nullptr);
#endif
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
	*p_columns = columns_;
	return values_;
}

inline void SparseVector::Strengths(uint32_t *p_nnz, uint32_t **p_columns, sv_value_t **p_strengths)
{
#if DEBUG
	// should be done building the vector
	if (!finished_)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strengths): sparse vector is not finished being built." << EidosTerminate(nullptr);
	if (value_type_ != SparseVectorDataType::kStrengths)
		EIDOS_TERMINATION << "ERROR (SparseVector::Strengths): sparse vector is not specialized for strengths." << EidosTerminate(nullptr);
#endif
	
	// return info; note that a non-null pointer is returned even if count==0
	*p_nnz = (uint32_t)nnz_;	// cast should be safe, the number of entries is 32-bit
	*p_columns = columns_;
	*p_strengths = values_;
}

std::ostream &operator<<(std::ostream &p_outstream, const SparseVector &p_vector);


#endif /* sparse_vector_h */
























