//
//  genome.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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

/*
 
 The class Genome represents a particular genome, defined as a vector of mutations.  Each individual in the simulation has a genome,
 which determines that individual's fitness (from the fitness effects of all of the mutations possessed).
 
 */

#ifndef __SLiM__genome__
#define __SLiM__genome__


#include <vector>

#include "mutation.h"
#include "slim_global.h"
#include "eidos_value.h"


// Genome now has an internal buffer that it can use to hold mutation pointers.  This makes every Genome object a bit bigger;
// with 64-bit pointers, a buffer big enough to hold four pointers is 32 bytes, ouch.  But avoiding the malloc overhead is
// worth it, for simulations with few mutations; and for simulations with many mutations, the 32-byte overhead is background noise.
#define GENOME_MUT_BUFFER_SIZE	4


class Genome : public EidosObjectElement
{
	// This class has a restricted copying policy; see below
	
	EidosValue *self_value_ = nullptr;					// OWNED POINTER: cached EidosValue object for speed
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	GenomeType genome_type_ = GenomeType::kAutosome;	// SEX ONLY: the type of chromosome represented by this genome
	
	int mutation_count_ = 0;							// the number of entries presently in mutations_
	int mutation_capacity_ = GENOME_MUT_BUFFER_SIZE;	// the capacity of mutations_; we start by using our internal buffer
	Mutation *(mutations_buffer_[GENOME_MUT_BUFFER_SIZE]);	// a built-in buffer to prevent the need for malloc with few mutations
	Mutation **mutations_ = mutations_buffer_;			// OWNED POINTER: a pointer to an array of pointers to const Mutation objects
														// note that mutations_ == nullptr indicates a null (i.e. placeholder) genome
	int64_t tag_value_;									// a user-defined tag value
	
#ifdef DEBUG
	static bool s_log_copy_and_assign_;					// true if logging is disabled (see below)
#endif
	
public:
	
	//
	//	This class should not be copied, in general, but the default copy constructor and assignment operator cannot be entirely
	//	disabled, because we want to keep instances of this class inside STL containers.  We therefore override the default copy
	//	constructor and the default assignment operator to log whenever they are called.  This is intended to reduce the risk of
	//	unintentional copying.  Logging can be disabled by bracketing with LogGenomeCopyAndAssign() when appropriate, or by
	//	using copy_from_genome(), which is the preferred way to intentionally copy a Genome.
	//
	Genome(const Genome &p_original);
	Genome& operator= (const Genome &p_original);
#ifdef DEBUG
	static bool LogGenomeCopyAndAssign(bool p_log);		// returns the old value; save and restore that value!
#endif
	
	Genome(void);											// default constructor; gives a non-null genome of type GenomeType::kAutosome
	Genome(GenomeType p_genome_type_, bool p_is_null);		// a constructor for parent/child genomes, particularly in the SEX ONLY case: species type and null/non-null
	~Genome(void);
	
	void NullGenomeAccessError(void) const;								// prints an error message, a stacktrace, and exits; called only for DEBUG
	
	inline bool IsNull(void) const										// returns true if the genome is a null (placeholder) genome, false otherwise
	{
		return (mutations_ == nullptr);
	}
	
	GenomeType GenomeType(void) const									// returns the type of the genome: automosomal, X chromosome, or Y chromosome
	{
		return genome_type_;
	}
	
	void RemoveFixedMutations(int p_fixed_count);						// Remove all mutations with a refcount of p_fixed_count, indicating that they have fixed
	
	inline Mutation *const & operator[] (int index) const			// [] returns a reference to a pointer to Mutation; this is the const-pointer variant
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		return mutations_[index];
	}
	
	inline Mutation *& operator[] (int index)						// [] returns a reference to a pointer to Mutation; this is the non-const-pointer variant
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		return mutations_[index];
	}
	
	inline int size(void)
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		return mutation_count_;
	}
	
	inline void clear(void)
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		mutation_count_ = 0;
	}
	
	inline bool contains_mutation(Mutation *p_mutation)
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		// This function does not assume that mutations are in sorted order, because we want to be able to use it with the mutation registry
		Mutation **position = begin_pointer();
		Mutation **end_position = end_pointer();
		
		for (; position != end_position; ++position)
			if (*position == p_mutation)
				return true;
		
		return false;
	}
	
	inline void pop_back(void)
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		if (mutation_count_ > 0)	// the standard says that popping an empty vector results in undefined behavior; this seems reasonable
			--mutation_count_;
	}
	
	inline void push_back(Mutation *p_mutation)
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		if (mutation_count_ == mutation_capacity_)
		{
			if (mutations_ == mutations_buffer_)
			{
				// we're allocating a malloced buffer for the first time, so we outgrew our internal buffer.  In this case,
				// let's jump by more than a factor of two, to try to avoid having to do repeated reallocs as we grow; we're
				// clearly running more than a minimal-size simulation.  The price paid is not huge, and the benefit is large.
				mutation_capacity_ = GENOME_MUT_BUFFER_SIZE * 8;
				mutations_ = (Mutation **)malloc(mutation_capacity_ * sizeof(Mutation*));
				
				memcpy(mutations_, mutations_buffer_, mutation_count_ * sizeof(Mutation*));
			}
			else
			{
				mutation_capacity_ <<= 1;		// double the number of pointers we can hold
				mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
			}
		}
		
		// Now we are guaranteed to have enough memory, so copy the pointer in
		// (unless malloc/realloc failed, which we're not going to worry about!)
		*(mutations_ + mutation_count_) = p_mutation;
		++mutation_count_;
	}
	
	inline void insert_sorted_mutation(Mutation *p_mutation)
	{
		// first push it back on the end, which deals with capacity issues
		push_back(p_mutation);
		
		// if it was our first element, then we're done; this would work anyway, but since it is extremely common let's short-circuit it
		if (mutation_count_ == 1)
			return;
		
		// then find the proper position for it
		Mutation **sort_position = begin_pointer();
		Mutation **end_position = end_pointer() - 1;		// the position of the newly added element
		
		for (sort_position = mutations_; sort_position != end_position; ++sort_position)
			if (CompareMutations(p_mutation, *sort_position))	// if (p_mutation->position_ < (*sort_position)->position_)
				break;
		
		// if we got all the way to the end, then the mutation belongs at the end, so we're done
		if (sort_position == end_position)
			return;
		
		// the new mutation has a position less than that at sort_position, so we need to move everybody upward
		memmove(sort_position + 1, sort_position, (char *)end_position - (char *)sort_position);
		
		// finally, put the mutation where it belongs
		*sort_position = p_mutation;
	}
	
	inline void insert_sorted_mutation_if_unique(Mutation *p_mutation)
	{
		// first push it back on the end, which deals with capacity issues
		push_back(p_mutation);
		
		// if it was our first element, then we're done; this would work anyway, but since it is extremely common let's short-circuit it
		if (mutation_count_ == 1)
			return;
		
		// then find the proper position for it
		Mutation **sort_position = begin_pointer();
		Mutation **end_position = end_pointer() - 1;		// the position of the newly added element
		
		for (sort_position = mutations_; sort_position != end_position; ++sort_position)
		{
			if (CompareMutations(p_mutation, *sort_position))	// if (p_mutation->position_ < (*sort_position)->position_)
			{
				break;
			}
			else if (p_mutation == *sort_position)
			{
				// We are only supposed to insert the mutation if it is unique, and apparently it is not; discard it off the end
				--mutation_count_;
				return;
			}
		}
		
		// if we got all the way to the end, then the mutation belongs at the end, so we're done
		if (sort_position == end_position)
			return;
		
		// the new mutation has a position less than that at sort_position, so we need to move everybody upward
		memmove(sort_position + 1, sort_position, (char *)end_position - (char *)sort_position);
		
		// finally, put the mutation where it belongs
		*sort_position = p_mutation;
	}
	
	inline void copy_from_genome(const Genome &p_source_genome)
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		if (p_source_genome.mutations_ == nullptr)
		{
			// p_original is a null genome, so make ourselves null too
			if (mutations_ != mutations_buffer_)
				free(mutations_);
			
			mutations_ = nullptr;
			mutation_capacity_ = 0;
			mutation_count_ = 0;
		}
		else
		{
			int source_mutation_count = p_source_genome.mutation_count_;
			
			// first we need to ensure that we have sufficient capacity
			if (source_mutation_count > mutation_capacity_)
			{
				mutation_capacity_ = p_source_genome.mutation_capacity_;		// just use the same capacity as the source
				
				// mutations_buffer_ is not malloced and cannot be realloced, so forget that we were using it
				if (mutations_ == mutations_buffer_)
					mutations_ = nullptr;
				
				mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
			}
			
			// then copy all pointers from the source to ourselves
			memcpy(mutations_, p_source_genome.mutations_, source_mutation_count * sizeof(Mutation*));
			mutation_count_ = source_mutation_count;
		}
		
		// and copy other state
		genome_type_ = p_source_genome.genome_type_;
	}
	
	inline Mutation **begin_pointer(void) const
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		return mutations_;
	}
	
	inline Mutation **end_pointer(void) const
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		return mutations_ + mutation_count_;
	}
	
	inline Mutation *& back(void) const				// returns a reference to a pointer to a const Mutation
	{
#ifdef DEBUG
		if (mutations_ == nullptr)
			NullGenomeAccessError();
#endif
		return *(mutations_ + (mutation_count_ - 1));
	}
	
	//
	// Eidos support
	//
	void GenerateCachedEidosValue(void);
	inline EidosValue *CachedEidosValue(void) { if (!self_value_) GenerateCachedEidosValue(); return self_value_; };
	
	virtual const std::string *ElementType(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual std::vector<std::string> ReadOnlyMembers(void) const;
	virtual std::vector<std::string> ReadWriteMembers(void) const;
	virtual bool MemberIsReadOnly(EidosGlobalStringID p_member_id) const;
	virtual EidosValue *GetValueForMember(EidosGlobalStringID p_member_id);
	virtual void SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value);
	
	virtual std::vector<std::string> Methods(void) const;
	virtual const EidosFunctionSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue *ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};

#endif /* defined(__SLiM__genome__) */




































































